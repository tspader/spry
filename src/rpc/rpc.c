#include "sp.h"
#include "yyjson.h"
#include "spry/rpc.h"

typedef struct spry_rpc_entry {
  sp_str_t name;
  spry_handler_fn_t fn;
  void* ctx;
  struct spry_rpc_entry* next;
} spry_rpc_entry_t;

struct spry_rpc {
  sp_mem_t mem;
  sp_mem_arena_t* request;
  sp_mem_t request_mem;
  bool dispatching;
  spry_endpoints_t endpoints;
  spry_rpc_entry_t* entries;
};

spry_rpc_t* spry_rpc_new(sp_mem_t mem, spry_endpoints_t endpoints) {
  spry_rpc_t* rpc = sp_alloc_type(mem, spry_rpc_t);
  *rpc = sp_zero_s(spry_rpc_t);
  rpc->mem = mem;
  rpc->request = sp_mem_arena_new(mem);
  rpc->request_mem = sp_mem_arena_as_allocator(rpc->request);
  rpc->endpoints = endpoints;
  return rpc;
}

sp_mem_t spry_rpc_mem(spry_rpc_t* rpc) {
  return rpc->mem;
}

sp_mem_t spry_request_mem(spry_rpc_t* rpc) {
  return rpc->request_mem;
}

spry_ui_t* spry_ui_begin(spry_rpc_t* rpc) {
  return spry_ui_new(rpc->request_mem);
}

void spry_rpc_register(spry_rpc_t* rpc, sp_str_t name, spry_handler_fn_t fn, void* ctx) {
  spry_rpc_entry_t* entry = sp_alloc_type(rpc->mem, spry_rpc_entry_t);
  *entry = sp_zero_s(spry_rpc_entry_t);
  entry->name = sp_str_copy(rpc->mem, name);
  entry->fn = fn;
  entry->ctx = ctx;
  entry->next = rpc->entries;
  rpc->entries = entry;
}

void spry_rpc_bind(spry_rpc_t* rpc, sp_str_t name, spry_handler_any_t fn, void* ctx, spry_handler_fn_t thunk) {
  spry_binding_t* binding = sp_alloc_type(rpc->mem, spry_binding_t);
  *binding = (spry_binding_t){ .rpc = rpc, .fn = fn, .ctx = ctx };
  spry_rpc_register(rpc, name, thunk, binding);
}

bool spry_rpc_parse(spry_binding_t* binding, const spry_args_t* args, const spry_ast_type_t* type, void* out, spry_reply_t* fault) {
  spry_ctx_t ctx;
  spry_ctx_init(&ctx, spry_request_mem(binding->rpc));
  if (spry_ast_parse(type, args->root, &ctx, out) == SPRY_OK) return true;
  *fault = spry_fault(binding->rpc, SPRY_CODE_INVALID, spry_issue_str(spry_request_mem(binding->rpc), &ctx.issues[0]));
  return false;
}

static spry_rpc_entry_t* spry_rpc_entry_find(spry_rpc_t* rpc, sp_str_t name) {
  for (spry_rpc_entry_t* entry = rpc->entries; entry; entry = entry->next) {
    if (sp_str_equal(entry->name, name)) return entry;
  }
  return SP_NULLPTR;
}

bool spry_rpc_check(spry_rpc_t* rpc) {
  bool ok = true;
  sp_da_for(rpc->endpoints, i) {
    if (!spry_rpc_entry_find(rpc, rpc->endpoints[i].key)) {
      sp_log("spry: endpoint '{}' declared but no handler registered", sp_fmt_str(rpc->endpoints[i].key));
      ok = false;
    }
  }
  for (spry_rpc_entry_t* entry = rpc->entries; entry; entry = entry->next) {
    if (!spry_endpoint_find(rpc->endpoints, entry->name)) {
      sp_log("spry: handler '{}' registered but not declared", sp_fmt_str(entry->name));
      ok = false;
    }
  }
  return ok;
}

spry_reply_t spry_ok(sp_str_t body) {
  return (spry_reply_t){ .outcome = DELIVER_OK, .body = body };
}

spry_reply_t spry_ok_ui(spry_ui_t* ui) {
  return spry_ok(spry_ui_write(ui, spry_ui_root(ui)));
}

spry_reply_t spry_fault_str(spry_rpc_t* rpc, spry_code_t code, sp_str_t message) {
  spry_fault_t fault = sp_zero_s(spry_fault_t);
  fault.code = code;
  fault.message = message;
  return (spry_reply_t){ .outcome = DELIVER_FAULT, .body = spry_fault_write(rpc->request_mem, &fault) };
}

spry_reply_t spry_fault_fmt(spry_rpc_t* rpc, spry_code_t code, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_r result = sp_fmt_mem_v(rpc->request_mem, sp_cstr_as_str(fmt), args);
  va_end(args);
  return spry_fault(rpc, code, result.value);
}

static spry_reply_t spry_fault_invalid(spry_rpc_t* rpc, spry_issues_t issues) {
  spry_fault_t fault = sp_zero_s(spry_fault_t);
  fault.code = SPRY_CODE_INVALID;
  fault.message = sp_str_lit("argument validation failed");
  fault.issues = issues;
  return (spry_reply_t){ .outcome = DELIVER_FAULT, .body = spry_fault_write(rpc->request_mem, &fault) };
}

static spry_reply_t spry_rpc_dispatch_request(spry_rpc_t* rpc, sp_str_t handler, sp_str_t body) {
  spry_rpc_entry_t* entry = spry_rpc_entry_find(rpc, handler);
  if (!entry) {
    return spry_fault_fmt(rpc, SPRY_CODE_MISSING, "unknown endpoint '{}'", sp_fmt_str(handler));
  }

  yyjson_doc* doc = yyjson_read(body.data, body.len, 0);
  if (!doc) {
    return spry_fault(rpc, SPRY_CODE_INVALID, "malformed JSON body");
  }

  const spry_endpoint_t* ep = spry_endpoint_find(rpc->endpoints, handler);
  if (ep) {
    spry_issues_t issues = spry_validate_args(rpc->request_mem, ep, yyjson_doc_get_root(doc));
    if (sp_da_size(issues)) {
      yyjson_doc_free(doc);
      return spry_fault_invalid(rpc, issues);
    }
  }

  spry_args_t args = { .root = yyjson_doc_get_root(doc) };
  spry_reply_t reply = entry->fn(entry->ctx, &args);
  yyjson_doc_free(doc);
  return reply;
}

spry_reply_t spry_rpc_dispatch(spry_rpc_t* rpc, sp_str_t handler, sp_str_t body) {
  SP_ASSERT(!rpc->dispatching);
  rpc->dispatching = true;
  sp_mem_arena_clear(rpc->request);
  spry_reply_t reply = spry_rpc_dispatch_request(rpc, handler, body);
  rpc->dispatching = false;
  return reply;
}

spry_reply_t spry_rpc_resolver(void* rpc, sp_str_t handler, sp_str_t body) {
  return spry_rpc_dispatch(rpc, handler, body);
}
