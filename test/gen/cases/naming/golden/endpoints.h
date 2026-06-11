#ifndef MY_APP2_ENDPOINTS_GEN_H
#define MY_APP2_ENDPOINTS_GEN_H

#include "spry/core.h"
#include "spry/rpc.h"

typedef struct {
  bool x;
} my_app2__foo_bar__baz__args_t;

static const spry_ast_field_t my_app2__foo_bar__baz__args_fields[] = {
  { .key = "x", .offset = offsetof(my_app2__foo_bar__baz__args_t, x), .type = &spry_bool_type, .required = true, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t my_app2__foo_bar__baz__args_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = my_app2__foo_bar__baz__args_fields, .count = 1 } };

typedef spry_reply_t (*my_app2__foo_bar__baz__fn_t)(void* ctx, const my_app2__foo_bar__baz__args_t* args);

static inline spry_reply_t my_app2__foo_bar__baz__thunk(void* ctx, const spry_args_t* args) {
  spry_binding_t* binding = ctx;
  my_app2__foo_bar__baz__args_t out = sp_zero_s(my_app2__foo_bar__baz__args_t);
  spry_reply_t fault;
  if (!spry_rpc_parse(binding, args, &my_app2__foo_bar__baz__args_type, &out, &fault)) return fault;
  return ((my_app2__foo_bar__baz__fn_t)binding->fn)(binding->ctx, &out);
}

static inline void my_app2_register__foo_bar__baz_(spry_rpc_t* rpc, my_app2__foo_bar__baz__fn_t fn, void* ctx) {
  spry_rpc_bind(rpc, sp_str_lit("_foo_bar__baz_"), (spry_handler_any_t)fn, ctx, my_app2__foo_bar__baz__thunk);
}

typedef struct {
  bool x;
} my_app2_v2_x9_args_t;

static const spry_ast_field_t my_app2_v2_x9_args_fields[] = {
  { .key = "x", .offset = offsetof(my_app2_v2_x9_args_t, x), .type = &spry_bool_type, .required = true, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t my_app2_v2_x9_args_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = my_app2_v2_x9_args_fields, .count = 1 } };

typedef spry_reply_t (*my_app2_v2_x9_fn_t)(void* ctx, const my_app2_v2_x9_args_t* args);

static inline spry_reply_t my_app2_v2_x9_thunk(void* ctx, const spry_args_t* args) {
  spry_binding_t* binding = ctx;
  my_app2_v2_x9_args_t out = sp_zero_s(my_app2_v2_x9_args_t);
  spry_reply_t fault;
  if (!spry_rpc_parse(binding, args, &my_app2_v2_x9_args_type, &out, &fault)) return fault;
  return ((my_app2_v2_x9_fn_t)binding->fn)(binding->ctx, &out);
}

static inline void my_app2_register_v2_x9(spry_rpc_t* rpc, my_app2_v2_x9_fn_t fn, void* ctx) {
  spry_rpc_bind(rpc, sp_str_lit("v2_x9"), (spry_handler_any_t)fn, ctx, my_app2_v2_x9_thunk);
}

typedef struct {
  bool x;
} my_app2_foo2bar_args_t;

static const spry_ast_field_t my_app2_foo2bar_args_fields[] = {
  { .key = "x", .offset = offsetof(my_app2_foo2bar_args_t, x), .type = &spry_bool_type, .required = true, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t my_app2_foo2bar_args_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = my_app2_foo2bar_args_fields, .count = 1 } };

typedef spry_reply_t (*my_app2_foo2bar_fn_t)(void* ctx, const my_app2_foo2bar_args_t* args);

static inline spry_reply_t my_app2_foo2bar_thunk(void* ctx, const spry_args_t* args) {
  spry_binding_t* binding = ctx;
  my_app2_foo2bar_args_t out = sp_zero_s(my_app2_foo2bar_args_t);
  spry_reply_t fault;
  if (!spry_rpc_parse(binding, args, &my_app2_foo2bar_args_type, &out, &fault)) return fault;
  return ((my_app2_foo2bar_fn_t)binding->fn)(binding->ctx, &out);
}

static inline void my_app2_register_foo2bar(spry_rpc_t* rpc, my_app2_foo2bar_fn_t fn, void* ctx) {
  spry_rpc_bind(rpc, sp_str_lit("foo2bar"), (spry_handler_any_t)fn, ctx, my_app2_foo2bar_thunk);
}

typedef struct {
  bool x;
} my_app2_switch_args_t;

static const spry_ast_field_t my_app2_switch_args_fields[] = {
  { .key = "x", .offset = offsetof(my_app2_switch_args_t, x), .type = &spry_bool_type, .required = true, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t my_app2_switch_args_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = my_app2_switch_args_fields, .count = 1 } };

typedef spry_reply_t (*my_app2_switch_fn_t)(void* ctx, const my_app2_switch_args_t* args);

static inline spry_reply_t my_app2_switch_thunk(void* ctx, const spry_args_t* args) {
  spry_binding_t* binding = ctx;
  my_app2_switch_args_t out = sp_zero_s(my_app2_switch_args_t);
  spry_reply_t fault;
  if (!spry_rpc_parse(binding, args, &my_app2_switch_args_type, &out, &fault)) return fault;
  return ((my_app2_switch_fn_t)binding->fn)(binding->ctx, &out);
}

static inline void my_app2_register_switch(spry_rpc_t* rpc, my_app2_switch_fn_t fn, void* ctx) {
  spry_rpc_bind(rpc, sp_str_lit("switch"), (spry_handler_any_t)fn, ctx, my_app2_switch_thunk);
}

typedef struct {
  my_app2__foo_bar__baz__fn_t _foo_bar__baz_;
  my_app2_v2_x9_fn_t v2_x9;
  my_app2_foo2bar_fn_t foo2bar;
  my_app2_switch_fn_t switch_;
} my_app2_handlers_t;

static inline void my_app2_register(spry_rpc_t* rpc, my_app2_handlers_t handlers, void* ctx) {
  if (handlers._foo_bar__baz_) my_app2_register__foo_bar__baz_(rpc, handlers._foo_bar__baz_, ctx);
  if (handlers.v2_x9) my_app2_register_v2_x9(rpc, handlers.v2_x9, ctx);
  if (handlers.foo2bar) my_app2_register_foo2bar(rpc, handlers.foo2bar, ctx);
  if (handlers.switch_) my_app2_register_switch(rpc, handlers.switch_, ctx);
}

#endif
