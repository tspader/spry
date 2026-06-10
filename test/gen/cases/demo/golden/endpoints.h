#ifndef DEMO_ENDPOINTS_GEN_H
#define DEMO_ENDPOINTS_GEN_H

#include "spry/core.h"
#include "spry/rpc.h"

typedef spry_reply_t (*demo_tables_fn_t)(void* ctx);

static inline spry_reply_t demo_tables_thunk(void* ctx, const spry_args_t* args) {
  spry_binding_t* binding = ctx;
  (void)args;
  return ((demo_tables_fn_t)binding->fn)(binding->ctx);
}

static inline void demo_register_tables(spry_rpc_t* rpc, demo_tables_fn_t fn, void* ctx) {
  spry_rpc_bind(rpc, sp_str_lit("tables"), (spry_handler_any_t)fn, ctx, demo_tables_thunk);
}

typedef struct {
  sp_str_t table;
} demo_open_table_args_t;

static const spry_ast_field_t demo_open_table_args_fields[] = {
  { .key = "table", .offset = offsetof(demo_open_table_args_t, table), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t demo_open_table_args_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = demo_open_table_args_fields, .count = 1 } };

typedef spry_reply_t (*demo_open_table_fn_t)(void* ctx, const demo_open_table_args_t* args);

static inline spry_reply_t demo_open_table_thunk(void* ctx, const spry_args_t* args) {
  spry_binding_t* binding = ctx;
  demo_open_table_args_t out = sp_zero_s(demo_open_table_args_t);
  spry_reply_t fault;
  if (!spry_rpc_parse(binding, args, &demo_open_table_args_type, &out, &fault)) return fault;
  return ((demo_open_table_fn_t)binding->fn)(binding->ctx, &out);
}

static inline void demo_register_open_table(spry_rpc_t* rpc, demo_open_table_fn_t fn, void* ctx) {
  spry_rpc_bind(rpc, sp_str_lit("open_table"), (spry_handler_any_t)fn, ctx, demo_open_table_thunk);
}

typedef struct {
  sp_str_t table;
  s32 rowid;
  sp_str_t column;
} demo_edit_cell_args_t;

static const spry_ast_field_t demo_edit_cell_args_fields[] = {
  { .key = "table", .offset = offsetof(demo_edit_cell_args_t, table), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "rowid", .offset = offsetof(demo_edit_cell_args_t, rowid), .type = &spry_s32_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "column", .offset = offsetof(demo_edit_cell_args_t, column), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t demo_edit_cell_args_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = demo_edit_cell_args_fields, .count = 3 } };

typedef spry_reply_t (*demo_edit_cell_fn_t)(void* ctx, const demo_edit_cell_args_t* args);

static inline spry_reply_t demo_edit_cell_thunk(void* ctx, const spry_args_t* args) {
  spry_binding_t* binding = ctx;
  demo_edit_cell_args_t out = sp_zero_s(demo_edit_cell_args_t);
  spry_reply_t fault;
  if (!spry_rpc_parse(binding, args, &demo_edit_cell_args_type, &out, &fault)) return fault;
  return ((demo_edit_cell_fn_t)binding->fn)(binding->ctx, &out);
}

static inline void demo_register_edit_cell(spry_rpc_t* rpc, demo_edit_cell_fn_t fn, void* ctx) {
  spry_rpc_bind(rpc, sp_str_lit("edit_cell"), (spry_handler_any_t)fn, ctx, demo_edit_cell_thunk);
}

typedef struct {
  sp_str_t table;
  s32 rowid;
  sp_str_t column;
  sp_str_t value;
} demo_save_cell_args_t;

static const spry_ast_field_t demo_save_cell_args_fields[] = {
  { .key = "table", .offset = offsetof(demo_save_cell_args_t, table), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "rowid", .offset = offsetof(demo_save_cell_args_t, rowid), .type = &spry_s32_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "column", .offset = offsetof(demo_save_cell_args_t, column), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "value", .offset = offsetof(demo_save_cell_args_t, value), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t demo_save_cell_args_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = demo_save_cell_args_fields, .count = 4 } };

typedef spry_reply_t (*demo_save_cell_fn_t)(void* ctx, const demo_save_cell_args_t* args);

static inline spry_reply_t demo_save_cell_thunk(void* ctx, const spry_args_t* args) {
  spry_binding_t* binding = ctx;
  demo_save_cell_args_t out = sp_zero_s(demo_save_cell_args_t);
  spry_reply_t fault;
  if (!spry_rpc_parse(binding, args, &demo_save_cell_args_type, &out, &fault)) return fault;
  return ((demo_save_cell_fn_t)binding->fn)(binding->ctx, &out);
}

static inline void demo_register_save_cell(spry_rpc_t* rpc, demo_save_cell_fn_t fn, void* ctx) {
  spry_rpc_bind(rpc, sp_str_lit("save_cell"), (spry_handler_any_t)fn, ctx, demo_save_cell_thunk);
}

typedef struct {
  sp_str_t sql;
} demo_exec_args_t;

static const spry_ast_field_t demo_exec_args_fields[] = {
  { .key = "sql", .offset = offsetof(demo_exec_args_t, sql), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t demo_exec_args_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = demo_exec_args_fields, .count = 1 } };

typedef spry_reply_t (*demo_exec_fn_t)(void* ctx, const demo_exec_args_t* args);

static inline spry_reply_t demo_exec_thunk(void* ctx, const spry_args_t* args) {
  spry_binding_t* binding = ctx;
  demo_exec_args_t out = sp_zero_s(demo_exec_args_t);
  spry_reply_t fault;
  if (!spry_rpc_parse(binding, args, &demo_exec_args_type, &out, &fault)) return fault;
  return ((demo_exec_fn_t)binding->fn)(binding->ctx, &out);
}

static inline void demo_register_exec(spry_rpc_t* rpc, demo_exec_fn_t fn, void* ctx) {
  spry_rpc_bind(rpc, sp_str_lit("exec"), (spry_handler_any_t)fn, ctx, demo_exec_thunk);
}

#endif
