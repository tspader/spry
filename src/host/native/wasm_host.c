#define SP_IMPLEMENTATION
#include "sp.h"
#include "wasm.h"
#include "abi.h"
#include "backend.h"
#include "wasm_host.h"

struct wasm_host {
  sp_mem_t mem;
  backend_t* backend;
  sp_da(void*) handles;
  wasm_engine_t* engine;
  wasm_store_t* store;
  wasm_module_t* module;
  wasm_instance_t* instance;
  wasm_memory_t* memory;
  wasm_func_t* fn_alloc;
  wasm_func_t* fn_render;
  wasm_func_t* fn_dispatch;
  wasm_func_t* fn_deliver;
};

static u32 host_put(wasm_host_t* h, void* node) {
  sp_da_push(h->handles, node);
  return (u32)sp_da_size(h->handles);
}

static void* host_ref(wasm_host_t* h, u32 handle) {
  return h->handles[handle - 1];
}

static sp_str_t host_str(wasm_host_t* h, u32 ptr, u32 len) {
  return (sp_str_t){ .data = (const c8*)(wasm_memory_data(h->memory) + ptr), .len = len };
}

static wasm_trap_t* cb_caps(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
  (void)args;
  wasm_host_t* h = env;
  results->data[0].kind = WASM_I32;
  results->data[0].of.i32 = (s32)h->backend->capabilities(h->backend->self);
  return SP_NULLPTR;
}

static wasm_trap_t* cb_create(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
  wasm_host_t* h = env;
  void* node = h->backend->create_element(h->backend->self, (u32)args->data[0].of.i32);
  results->data[0].kind = WASM_I32;
  results->data[0].of.i32 = (s32)host_put(h, node);
  return SP_NULLPTR;
}

static wasm_trap_t* cb_set_attr(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
  (void)results;
  wasm_host_t* h = env;
  h->backend->set_attr(h->backend->self, host_ref(h, (u32)args->data[0].of.i32),
                       (u32)args->data[1].of.i32, args->data[2].of.i32);
  return SP_NULLPTR;
}

static wasm_trap_t* cb_set_attr_str(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
  (void)results;
  wasm_host_t* h = env;
  sp_str_t value = host_str(h, (u32)args->data[2].of.i32, (u32)args->data[3].of.i32);
  h->backend->set_attr_str(h->backend->self, host_ref(h, (u32)args->data[0].of.i32),
                           (u32)args->data[1].of.i32, value);
  return SP_NULLPTR;
}

static wasm_trap_t* cb_append(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
  (void)results;
  wasm_host_t* h = env;
  h->backend->append_child(h->backend->self, host_ref(h, (u32)args->data[0].of.i32),
                           host_ref(h, (u32)args->data[1].of.i32));
  return SP_NULLPTR;
}

static wasm_trap_t* cb_set_root(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
  (void)results;
  wasm_host_t* h = env;
  h->backend->set_root(h->backend->self, host_ref(h, (u32)args->data[0].of.i32));
  return SP_NULLPTR;
}

static wasm_trap_t* cb_on_event(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
  (void)results;
  wasm_host_t* h = env;
  h->backend->on_event(h->backend->self, host_ref(h, (u32)args->data[0].of.i32),
                       (u32)args->data[1].of.i32, (u32)args->data[2].of.i32);
  return SP_NULLPTR;
}

static wasm_trap_t* cb_submit(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
  (void)results;
  wasm_host_t* h = env;
  sp_str_t action = host_str(h, (u32)args->data[1].of.i32, (u32)args->data[2].of.i32);
  sp_str_t body = host_str(h, (u32)args->data[3].of.i32, (u32)args->data[4].of.i32);
  h->backend->submit(h->backend->self, (u32)args->data[0].of.i32, action, body);
  return SP_NULLPTR;
}

static wasm_trap_t* cb_clear_children(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
  (void)results;
  wasm_host_t* h = env;
  h->backend->clear_children(h->backend->self, host_ref(h, (u32)args->data[0].of.i32));
  return SP_NULLPTR;
}

static wasm_trap_t* cb_get_value(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
  wasm_host_t* h = env;
  c8* out = (c8*)(wasm_memory_data(h->memory) + (u32)args->data[1].of.i32);
  u32 cap = (u32)args->data[2].of.i32;
  u32 n = h->backend->get_value(h->backend->self, host_ref(h, (u32)args->data[0].of.i32), out, cap);
  results->data[0].kind = WASM_I32;
  results->data[0].of.i32 = (s32)n;
  return SP_NULLPTR;
}

static wasm_trap_t* cb_fatal(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
  (void)results;
  wasm_host_t* h = env;
  sp_str_t msg = host_str(h, (u32)args->data[0].of.i32, (u32)args->data[1].of.i32);
  h->backend->fatal(h->backend->self, msg);
  return SP_NULLPTR;
}

typedef struct {
  const c8* name;
  wasm_func_callback_with_env_t cb;
  u32 nparams;
  u32 nresults;
} host_fn_def_t;

static const host_fn_def_t HOST_FNS[] = {
  { "capabilities",   cb_caps,           0, 1 },
  { "create",         cb_create,         1, 1 },
  { "set_attr",       cb_set_attr,       3, 0 },
  { "set_attr_str",   cb_set_attr_str,   4, 0 },
  { "append_child",   cb_append,         2, 0 },
  { "set_root",       cb_set_root,       1, 0 },
  { "on_event",       cb_on_event,       3, 0 },
  { "submit",         cb_submit,         5, 0 },
  { "clear_children", cb_clear_children, 1, 0 },
  { "get_value",      cb_get_value,      3, 1 },
  { "fatal",          cb_fatal,          2, 0 },
};

static const u32 HOST_FN_COUNT = (u32)sp_carr_len(HOST_FNS);

static wasm_functype_t* functype_i32(u32 nparams, u32 nresults) {
  wasm_valtype_t* params[8];
  wasm_valtype_t* results[8];
  sp_for(i, nparams) params[i] = wasm_valtype_new(WASM_I32);
  sp_for(i, nresults) results[i] = wasm_valtype_new(WASM_I32);

  wasm_valtype_vec_t pv, rv;
  if (nparams) wasm_valtype_vec_new(&pv, nparams, params); else wasm_valtype_vec_new_empty(&pv);
  if (nresults) wasm_valtype_vec_new(&rv, nresults, results); else wasm_valtype_vec_new_empty(&rv);
  return wasm_functype_new(&pv, &rv);
}

static bool name_eq(const wasm_name_t* name, const c8* s) {
  sp_str_t a = { .data = name->data, .len = (u32)name->size };
  return sp_str_equal_cstr(a, s);
}

wasm_host_t* wasm_host_new(sp_mem_t mem, sp_str_t wasm_path) {
  wasm_host_t* h = sp_alloc(mem, sizeof(wasm_host_t));
  *h = sp_zero_s(wasm_host_t);
  h->mem = mem;
  h->backend = SP_NULLPTR;
  h->handles = sp_da_new(mem, void*);

  sp_str_t wasm_bytes;
  if (sp_io_read_file(mem, wasm_path, &wasm_bytes) != SP_OK) {
    sp_log("wasm_host: failed to read {}", sp_fmt_str(wasm_path));
    return SP_NULLPTR;
  }

  h->engine = wasm_engine_new();
  h->store = wasm_store_new(h->engine);

  wasm_byte_vec_t binary;
  wasm_byte_vec_new_uninitialized(&binary, wasm_bytes.len);
  sp_mem_copy(binary.data, wasm_bytes.data, wasm_bytes.len);

  h->module = wasm_module_new(h->store, &binary);
  wasm_byte_vec_delete(&binary);
  if (!h->module) {
    sp_log("wasm_host: failed to compile module");
    return SP_NULLPTR;
  }

  wasm_func_t* funcs[HOST_FN_COUNT];
  sp_for(i, HOST_FN_COUNT) {
    wasm_functype_t* ft = functype_i32(HOST_FNS[i].nparams, HOST_FNS[i].nresults);
    funcs[i] = wasm_func_new_with_env(h->store, ft, HOST_FNS[i].cb, h, SP_NULLPTR);
    wasm_functype_delete(ft);
  }

  wasm_importtype_vec_t import_types;
  wasm_module_imports(h->module, &import_types);

  wasm_extern_t** externs = sp_alloc(mem, sizeof(wasm_extern_t*) * import_types.size);
  sp_for(i, import_types.size) {
    const wasm_name_t* want = wasm_importtype_name(import_types.data[i]);
    wasm_func_t* match = SP_NULLPTR;
    sp_for(j, HOST_FN_COUNT) {
      if (name_eq(want, HOST_FNS[j].name)) { match = funcs[j]; break; }
    }
    if (!match) {
      sp_str_t n = { .data = want->data, .len = (u32)want->size };
      sp_log("wasm_host: runtime needs import '{}' which the host does not provide", sp_fmt_str(n));
      return SP_NULLPTR;
    }
    externs[i] = wasm_func_as_extern(match);
  }

  wasm_extern_vec_t import_object = { .size = import_types.size, .data = externs };
  wasm_trap_t* trap = SP_NULLPTR;
  h->instance = wasm_instance_new(h->store, h->module, &import_object, &trap);
  wasm_importtype_vec_delete(&import_types);
  if (!h->instance) {
    sp_log("wasm_host: instantiation failed");
    return SP_NULLPTR;
  }

  wasm_extern_vec_t exports;
  wasm_instance_exports(h->instance, &exports);
  wasm_exporttype_vec_t export_types;
  wasm_module_exports(h->module, &export_types);
  sp_for(i, exports.size) {
    const wasm_name_t* name = wasm_exporttype_name(export_types.data[i]);
    if (name_eq(name, "alloc")) h->fn_alloc = wasm_extern_as_func(exports.data[i]);
    else if (name_eq(name, "render")) h->fn_render = wasm_extern_as_func(exports.data[i]);
    else if (name_eq(name, "dispatch")) h->fn_dispatch = wasm_extern_as_func(exports.data[i]);
    else if (name_eq(name, "deliver")) h->fn_deliver = wasm_extern_as_func(exports.data[i]);
    else if (name_eq(name, "memory")) h->memory = wasm_extern_as_memory(exports.data[i]);
  }
  wasm_exporttype_vec_delete(&export_types);

  if (!h->fn_alloc || !h->fn_render || !h->fn_dispatch || !h->fn_deliver || !h->memory) {
    sp_log("wasm_host: runtime is missing required exports");
    return SP_NULLPTR;
  }

  return h;
}

void wasm_host_set_backend(wasm_host_t* h, backend_t* backend) {
  h->backend = backend;
}

static u32 wasm_host_copy_in(wasm_host_t* h, sp_str_t bytes) {
  wasm_val_t args[1] = { { .kind = WASM_I32, .of = { .i32 = (s32)bytes.len } } };
  wasm_val_t res[1];
  wasm_val_vec_t av = { .size = 1, .data = args };
  wasm_val_vec_t rv = { .size = 1, .data = res };
  if (wasm_func_call(h->fn_alloc, &av, &rv)) { sp_log("wasm_host: alloc trapped"); return 0; }
  u32 ptr = (u32)res[0].of.i32;
  sp_mem_copy(wasm_memory_data(h->memory) + ptr, bytes.data, bytes.len);
  return ptr;
}

static void wasm_host_dispatch(void* ctx, u32 token) {
  wasm_host_t* h = ctx;
  wasm_val_t args[1] = { { .kind = WASM_I32, .of = { .i32 = (s32)token } } };
  wasm_val_vec_t av = { .size = 1, .data = args };
  wasm_val_vec_t rv = { .size = 0, .data = SP_NULLPTR };
  if (wasm_func_call(h->fn_dispatch, &av, &rv)) sp_log("wasm_host: dispatch trapped");
}

static void wasm_host_deliver(void* ctx, u32 token, sp_str_t json) {
  wasm_host_t* h = ctx;
  u32 ptr = wasm_host_copy_in(h, json);
  wasm_val_t args[3] = {
    { .kind = WASM_I32, .of = { .i32 = (s32)token } },
    { .kind = WASM_I32, .of = { .i32 = (s32)ptr } },
    { .kind = WASM_I32, .of = { .i32 = (s32)json.len } },
  };
  wasm_val_vec_t av = { .size = 3, .data = args };
  wasm_val_vec_t rv = { .size = 0, .data = SP_NULLPTR };
  if (wasm_func_call(h->fn_deliver, &av, &rv)) sp_log("wasm_host: deliver trapped");
}

host_iface_t wasm_host_iface(wasm_host_t* h) {
  return (host_iface_t){
    .ctx = h,
    .dispatch = wasm_host_dispatch,
    .deliver = wasm_host_deliver,
  };
}

s32 wasm_host_render(wasm_host_t* h, sp_str_t tree) {
  sp_da_clear(h->handles);

  u32 ptr = wasm_host_copy_in(h, tree);
  wasm_val_t args[2] = {
    { .kind = WASM_I32, .of = { .i32 = (s32)ptr } },
    { .kind = WASM_I32, .of = { .i32 = (s32)tree.len } },
  };
  wasm_val_t res[1];
  wasm_val_vec_t av = { .size = 2, .data = args };
  wasm_val_vec_t rv = { .size = 1, .data = res };
  if (wasm_func_call(h->fn_render, &av, &rv)) { sp_log("wasm_host: render trapped"); return -1; }

  return (s32)res[0].of.i32;
}
