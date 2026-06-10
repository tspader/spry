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
  spry_rpc_entry_t* entries;
};

spry_rpc_t* spry_rpc_new(sp_mem_t mem) {
  spry_rpc_t* rpc = sp_alloc_type(mem, spry_rpc_t);
  *rpc = sp_zero_s(spry_rpc_t);
  rpc->mem = mem;
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

static sp_str_t rpc_copy_yyjson(spry_rpc_t* rpc, const c8* data, size_t len) {
  return sp_str_copy(rpc->mem, sp_str(data, (u32)len));
}

spry_reply_t spry_ok(sp_str_t body) {
  return (spry_reply_t){ .outcome = DELIVER_OK, .body = body };
}

spry_reply_t spry_ok_doc(spry_rpc_t* rpc, yyjson_mut_doc* doc) {
  size_t len = 0;
  c8* out = yyjson_mut_write(doc, 0, &len);
  if (!out) {
    yyjson_mut_doc_free(doc);
    return spry_fault(rpc, "failed", sp_str_lit("response serialization failed"));
  }
  sp_str_t body = rpc_copy_yyjson(rpc, out, len);
  free(out);
  yyjson_mut_doc_free(doc);
  return spry_ok(body);
}

spry_reply_t spry_fault(spry_rpc_t* rpc, const c8* code, sp_str_t message) {
  yyjson_mut_doc* doc = yyjson_mut_doc_new(SP_NULLPTR);
  yyjson_mut_val* root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);
  yyjson_mut_obj_add_strcpy(doc, root, "code", code);
  if (!sp_str_empty(message)) {
    yyjson_mut_obj_add_strncpy(doc, root, "message", message.data, message.len);
  }
  size_t len = 0;
  c8* out = yyjson_mut_write(doc, 0, &len);
  sp_str_t body = out ? rpc_copy_yyjson(rpc, out, len) : sp_str_lit("{\"code\":\"failed\"}");
  if (out) free(out);
  yyjson_mut_doc_free(doc);
  return (spry_reply_t){ .outcome = DELIVER_FAULT, .body = body };
}

spry_reply_t spry_rpc_dispatch(spry_rpc_t* rpc, sp_str_t handler, sp_str_t body) {
  spry_rpc_entry_t* entry = rpc->entries;
  for (; entry; entry = entry->next) {
    if (sp_str_equal(entry->name, handler)) break;
  }
  if (!entry) {
    return spry_fault(rpc, "missing", sp_fmt(rpc->mem, "unknown endpoint '{}'", sp_fmt_str(handler)).value);
  }

  yyjson_doc* doc = yyjson_read(body.data, body.len, 0);
  if (!doc) {
    return spry_fault(rpc, "invalid", sp_str_lit("malformed JSON body"));
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
