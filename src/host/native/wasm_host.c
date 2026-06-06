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
  { "capabilities",  cb_caps,         0, 1 },
  { "create",        cb_create,       1, 1 },
  { "set_attr",      cb_set_attr,     3, 0 },
  { "set_attr_str",  cb_set_attr_str, 4, 0 },
  { "append_child",  cb_append,       2, 0 },
  { "set_root",      cb_set_root,      1, 0 },
  { "fatal",         cb_fatal,        2, 0 },
};

static const u32 HOST_FN_COUNT = (u32)(sizeof(HOST_FNS) / sizeof(HOST_FNS[0]));

static wasm_functype_t* functype_i32(u32 nparams, u32 nresults) {
  wasm_valtype_t* params[8];
  wasm_valtype_t* results[8];
  for (u32 i = 0; i < nparams; i++) params[i] = wasm_valtype_new(WASM_I32);
  for (u32 i = 0; i < nresults; i++) results[i] = wasm_valtype_new(WASM_I32);

  wasm_valtype_vec_t pv, rv;
  if (nparams) wasm_valtype_vec_new(&pv, nparams, params); else wasm_valtype_vec_new_empty(&pv);
  if (nresults) wasm_valtype_vec_new(&rv, nresults, results); else wasm_valtype_vec_new_empty(&rv);
  return wasm_functype_new(&pv, &rv);
}

static bool name_eq(const wasm_name_t* name, const c8* s) {
  sp_str_t a = { .data = name->data, .len = (u32)name->size };
  return sp_str_equal_cstr(a, s);
}

wasm_host_t* wasm_host_new(sp_mem_t mem, sp_str_t wasm_path, backend_t* backend) {
  wasm_host_t* h = sp_alloc(mem, sizeof(wasm_host_t));
  *h = sp_zero_s(wasm_host_t);
  h->mem = mem;
  h->backend = backend;
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
  memcpy(binary.data, wasm_bytes.data, wasm_bytes.len);

  h->module = wasm_module_new(h->store, &binary);
  wasm_byte_vec_delete(&binary);
  if (!h->module) {
    sp_log("wasm_host: failed to compile module");
    return SP_NULLPTR;
  }

  wasm_func_t* funcs[HOST_FN_COUNT];
  for (u32 i = 0; i < HOST_FN_COUNT; i++) {
    wasm_functype_t* ft = functype_i32(HOST_FNS[i].nparams, HOST_FNS[i].nresults);
    funcs[i] = wasm_func_new_with_env(h->store, ft, HOST_FNS[i].cb, h, SP_NULLPTR);
    wasm_functype_delete(ft);
  }

  wasm_importtype_vec_t import_types;
  wasm_module_imports(h->module, &import_types);

  wasm_extern_t** externs = sp_alloc(mem, sizeof(wasm_extern_t*) * import_types.size);
  for (u32 i = 0; i < import_types.size; i++) {
    const wasm_name_t* want = wasm_importtype_name(import_types.data[i]);
    wasm_func_t* match = SP_NULLPTR;
    for (u32 j = 0; j < HOST_FN_COUNT; j++) {
      if (name_eq(want, HOST_FNS[j].name)) { match = funcs[j]; break; }
    }
    if (!match) {
      sp_str_t n = { .data = want->data, .len = (u32)want->size };
      sp_log("wasm_host: runtime needs import '{}' which the host does not provide", sp_fmt_str(n));
      return SP_NULLPTR;
    }
    externs[i] = wasm_func_as_extern(match);
  }

  wasm_extern_vec_t import_object = { import_types.size, externs };
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
  for (u32 i = 0; i < exports.size; i++) {
    const wasm_name_t* name = wasm_exporttype_name(export_types.data[i]);
    if (name_eq(name, "alloc")) h->fn_alloc = wasm_extern_as_func(exports.data[i]);
    else if (name_eq(name, "render")) h->fn_render = wasm_extern_as_func(exports.data[i]);
    else if (name_eq(name, "memory")) h->memory = wasm_extern_as_memory(exports.data[i]);
  }
  wasm_exporttype_vec_delete(&export_types);

  if (!h->fn_alloc || !h->fn_render || !h->memory) {
    sp_log("wasm_host: runtime is missing alloc/render/memory exports");
    return SP_NULLPTR;
  }

  return h;
}

s32 wasm_host_render(wasm_host_t* h, sp_str_t tree) {
  sp_da_clear(h->handles);

  wasm_val_t alloc_args[1] = { { .kind = WASM_I32, .of = { .i32 = (s32)tree.len } } };
  wasm_val_t alloc_res[1];
  wasm_val_vec_t aav = { 1, alloc_args };
  wasm_val_vec_t arv = { 1, alloc_res };
  if (wasm_func_call(h->fn_alloc, &aav, &arv)) { sp_log("wasm_host: alloc trapped"); return -1; }
  u32 ptr = (u32)alloc_res[0].of.i32;

  byte_t* base = wasm_memory_data(h->memory);
  memcpy(base + ptr, tree.data, tree.len);

  wasm_val_t render_args[2] = {
    { .kind = WASM_I32, .of = { .i32 = (s32)ptr } },
    { .kind = WASM_I32, .of = { .i32 = (s32)tree.len } },
  };
  wasm_val_t render_res[1];
  wasm_val_vec_t rav = { 2, render_args };
  wasm_val_vec_t rrv = { 1, render_res };
  if (wasm_func_call(h->fn_render, &rav, &rrv)) { sp_log("wasm_host: render trapped"); return -1; }

  return (s32)render_res[0].of.i32;
}
