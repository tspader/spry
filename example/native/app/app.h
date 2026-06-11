#ifndef DEMO_H
#define DEMO_H

#include "demo.endpoints.gen.h"

typedef struct sqlite3 sqlite3;

typedef struct {
  sqlite3* db;
  spry_rpc_t* rpc;
} demo_ctx_t;

demo_ctx_t* demo_new(sp_mem_t mem, sqlite3* db, spry_endpoints_t endpoints);

spry_reply_t ep_tables(void* ctx);
spry_reply_t ep_open_table(void* ctx, const demo_open_table_args_t* args);
spry_reply_t ep_edit_cell(void* ctx, const demo_edit_cell_args_t* args);
spry_reply_t ep_save_cell(void* ctx, const demo_save_cell_args_t* args);
spry_reply_t ep_exec(void* ctx, const demo_exec_args_t* args);

spry_reply_t render_table_as_grid(demo_ctx_t* app, sp_str_t table);
spry_reply_t missing_table(demo_ctx_t* app, sp_str_t table);

#endif
