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
  spry_endpoints_t endpoints;
  spry_rpc_entry_t* entries;
};

spry_rpc_t* spry_rpc_new(sp_mem_t mem, spry_endpoints_t endpoints) {
  spry_rpc_t* rpc = sp_alloc_type(mem, spry_rpc_t);
  *rpc = sp_zero_s(spry_rpc_t);
  rpc->mem = mem;
  rpc->endpoints = endpoints;
  return rpc;
}

sp_mem_t spry_rpc_mem(spry_rpc_t* rpc) {
  return rpc->mem;
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

spry_reply_t spry_ok_ui(spry_rpc_t* rpc, spry_ui_t* ui, u32 root) {
  (void)rpc;
  return spry_ok(spry_ui_write(ui, root));
}

spry_reply_t spry_fault(spry_rpc_t* rpc, spry_code_t code, sp_str_t message) {
  spry_fault_t fault = sp_zero_s(spry_fault_t);
  fault.code = code;
  fault.message = message;
  return (spry_reply_t){ .outcome = DELIVER_FAULT, .body = spry_fault_write(rpc->mem, &fault) };
}

static spry_reply_t spry_fault_invalid(spry_rpc_t* rpc, spry_issues_t issues) {
  spry_fault_t fault = sp_zero_s(spry_fault_t);
  fault.code = SPRY_CODE_INVALID;
  fault.message = sp_str_lit("argument validation failed");
  fault.issues = issues;
  return (spry_reply_t){ .outcome = DELIVER_FAULT, .body = spry_fault_write(rpc->mem, &fault) };
}

spry_reply_t spry_rpc_dispatch(spry_rpc_t* rpc, sp_str_t handler, sp_str_t body) {
  spry_rpc_entry_t* entry = spry_rpc_entry_find(rpc, handler);
  if (!entry) {
    return spry_fault(rpc, SPRY_CODE_MISSING, sp_fmt(rpc->mem, "unknown endpoint '{}'", sp_fmt_str(handler)).value);
  }

  yyjson_doc* doc = yyjson_read(body.data, body.len, 0);
  if (!doc) {
    return spry_fault(rpc, SPRY_CODE_INVALID, sp_str_lit("malformed JSON body"));
  }

  const spry_endpoint_t* ep = spry_endpoint_find(rpc->endpoints, handler);
  if (ep) {
    spry_issues_t issues = spry_validate_args(rpc->mem, ep, yyjson_doc_get_root(doc));
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

spry_reply_t spry_rpc_resolver(void* rpc, sp_str_t handler, sp_str_t body) {
  return spry_rpc_dispatch(rpc, handler, body);
}

bool spry_args_str(const spry_args_t* args, const c8* name, sp_str_t* out) {
  yyjson_val* val = yyjson_obj_get(args->root, name);
  if (!yyjson_is_str(val)) return false;
  *out = sp_str(yyjson_get_str(val), (u32)yyjson_get_len(val));
  return true;
}

bool spry_args_s64(const spry_args_t* args, const c8* name, s64* out) {
  yyjson_val* val = yyjson_obj_get(args->root, name);
  if (!yyjson_is_int(val)) return false;
  *out = yyjson_get_sint(val);
  return true;
}

bool spry_args_f64(const spry_args_t* args, const c8* name, f64* out) {
  yyjson_val* val = yyjson_obj_get(args->root, name);
  if (!yyjson_is_num(val)) return false;
  *out = yyjson_get_num(val);
  return true;
}

bool spry_args_bool(const spry_args_t* args, const c8* name, bool* out) {
  yyjson_val* val = yyjson_obj_get(args->root, name);
  if (!yyjson_is_bool(val)) return false;
  *out = yyjson_get_bool(val);
  return true;
}

sp_str_t spry_arg_str(const spry_args_t* args, const c8* name) {
  sp_str_t out = sp_zero_s(sp_str_t);
  spry_args_str(args, name, &out);
  return out;
}

s64 spry_arg_s64(const spry_args_t* args, const c8* name) {
  s64 out = 0;
  spry_args_s64(args, name, &out);
  return out;
}

f64 spry_arg_f64(const spry_args_t* args, const c8* name) {
  f64 out = 0;
  spry_args_f64(args, name, &out);
  return out;
}

bool spry_arg_bool(const spry_args_t* args, const c8* name) {
  bool out = false;
  spry_args_bool(args, name, &out);
  return out;
}
