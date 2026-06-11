#ifndef SPRY_RPC_H
#define SPRY_RPC_H

#include "sp.h"
#include "yyjson.h"
#include "spry/core.h"
#include "spry/ui.h"
#include "spry/backend/backend.h"

typedef struct spry_rpc spry_rpc_t;

typedef struct {
  yyjson_val* root;
} spry_args_t;

typedef spry_reply_t (*spry_handler_fn_t)(void* ctx, const spry_args_t* args);
typedef spry_reply_t (*spry_handler_any_t)(void);

typedef struct {
  spry_rpc_t* rpc;
  spry_handler_any_t fn;
  void* ctx;
} spry_binding_t;

spry_rpc_t*  spry_rpc_new(sp_mem_t mem, spry_endpoints_t endpoints);
void         spry_rpc_register(spry_rpc_t* rpc, sp_str_t name, spry_handler_fn_t fn, void* ctx);
void         spry_rpc_bind(spry_rpc_t* rpc, sp_str_t name, spry_handler_any_t fn, void* ctx, spry_handler_fn_t thunk);
bool         spry_rpc_parse(spry_binding_t* binding, const spry_args_t* args, const spry_ast_type_t* type, void* out, spry_reply_t* fault);
bool         spry_rpc_check(spry_rpc_t* rpc);
spry_reply_t spry_rpc_dispatch(spry_rpc_t* rpc, sp_str_t handler, sp_str_t body);
spry_reply_t spry_rpc_resolver(void* rpc, sp_str_t handler, sp_str_t body);
sp_mem_t     spry_rpc_mem(spry_rpc_t* rpc);

sp_str_t spry_arg_str(const spry_args_t* args, const c8* name);
s64      spry_arg_s64(const spry_args_t* args, const c8* name);
f64      spry_arg_f64(const spry_args_t* args, const c8* name);
bool     spry_arg_bool(const spry_args_t* args, const c8* name);

bool spry_args_str(const spry_args_t* args, const c8* name, sp_str_t* out);
bool spry_args_s64(const spry_args_t* args, const c8* name, s64* out);
bool spry_args_f64(const spry_args_t* args, const c8* name, f64* out);
bool spry_args_bool(const spry_args_t* args, const c8* name, bool* out);

spry_reply_t spry_ok(sp_str_t body);
spry_reply_t spry_ok_ui(spry_rpc_t* rpc, spry_ui_t* ui);
spry_reply_t spry_fault(spry_rpc_t* rpc, spry_code_t code, sp_str_t message);
spry_reply_t spry_fault_fmt(sp_mem_t mem, spry_rpc_t* rpc, spry_code_t code, const c8* fmt, ...);

#endif
