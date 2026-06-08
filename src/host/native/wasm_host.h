#ifndef WASM_HOST_H
#define WASM_HOST_H

#include "sp.h"
#include "backend.h"

typedef struct wasm_host wasm_host_t;

wasm_host_t* wasm_host_new(sp_mem_t mem, sp_str_t wasm_path);
void         wasm_host_set_backend(wasm_host_t* host, backend_t* backend);
host_iface_t wasm_host_iface(wasm_host_t* host);
s32          wasm_host_render(wasm_host_t* host, sp_str_t tree_json);

#endif
