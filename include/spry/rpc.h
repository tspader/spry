#ifndef SPRY_RPC_H
#define SPRY_RPC_H

#include "sp.h"
#include "yyjson.h"
#include "spry/backend/backend.h"

typedef struct spry_rpc spry_rpc_t;

typedef struct {
  yyjson_val* root;
} spry_args_t;

typedef spry_reply_t (*spry_handler_fn_t)(void* ctx, const spry_args_t* args);

spry_rpc_t*  spry_rpc_new(sp_mem_t mem);
void         spry_rpc_register(spry_rpc_t* rpc, sp_str_t name, spry_handler_fn_t fn, void* ctx);
spry_reply_t spry_rpc_dispatch(spry_rpc_t* rpc, sp_str_t handler, sp_str_t body);
spry_reply_t spry_rpc_resolver(void* rpc, sp_str_t handler, sp_str_t body);
sp_mem_t     spry_rpc_mem(spry_rpc_t* rpc);

bool spry_args_str(const spry_args_t* args, const c8* name, sp_str_t* out);
bool spry_args_s64(const spry_args_t* args, const c8* name, s64* out);
bool spry_args_f64(const spry_args_t* args, const c8* name, f64* out);
bool spry_args_bool(const spry_args_t* args, const c8* name, bool* out);

spry_reply_t spry_ok(sp_str_t body);
spry_reply_t spry_ok_doc(spry_rpc_t* rpc, yyjson_mut_doc* doc);
spry_reply_t spry_fault(spry_rpc_t* rpc, const c8* code, sp_str_t message);

#endif
