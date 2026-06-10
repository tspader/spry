#include <sqlite3.h>
#include "sp.h"
#include "yyjson.h"
#include "spry/rpc.h"
#include "handlers.h"

#define ROW_LIMIT 20

static yyjson_mut_val* mk_node(yyjson_mut_doc* doc, const c8* kind) {
  yyjson_mut_val* node = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_strcpy(doc, node, "kind", kind);
  return node;
}

static yyjson_mut_val* mk_props(yyjson_mut_doc* doc, yyjson_mut_val* node) {
  yyjson_mut_val* props = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_val(doc, node, "props", props);
  return props;
}

static yyjson_mut_val* mk_children(yyjson_mut_doc* doc, yyjson_mut_val* node) {
  yyjson_mut_val* children = yyjson_mut_arr(doc);
  yyjson_mut_obj_add_val(doc, node, "children", children);
  return children;
}

static yyjson_mut_val* mk_box(yyjson_mut_doc* doc, const c8* direction, s32 gap) {
  yyjson_mut_val* node = mk_node(doc, "box");
  yyjson_mut_val* props = mk_props(doc, node);
  yyjson_mut_obj_add_strcpy(doc, props, "direction", direction);
  yyjson_mut_obj_add_int(doc, props, "gap", gap);
  return node;
}

static yyjson_mut_val* mk_text(yyjson_mut_doc* doc, sp_str_t value) {
  yyjson_mut_val* node = mk_node(doc, "text");
  yyjson_mut_val* props = mk_props(doc, node);
  yyjson_mut_obj_add_strncpy(doc, props, "text", value.data, value.len);
  return node;
}

static yyjson_mut_val* mk_button(yyjson_mut_doc* doc, sp_str_t label) {
  yyjson_mut_val* node = mk_node(doc, "button");
  yyjson_mut_val* props = mk_props(doc, node);
  yyjson_mut_obj_add_strncpy(doc, props, "text", label.data, label.len);
  return node;
}

static yyjson_mut_val* mk_input(yyjson_mut_doc* doc, const c8* name, sp_str_t value) {
  yyjson_mut_val* node = mk_node(doc, "input");
  yyjson_mut_val* props = mk_props(doc, node);
  yyjson_mut_obj_add_strcpy(doc, props, "name", name);
  yyjson_mut_obj_add_strncpy(doc, props, "value", value.data, value.len);
  return node;
}

static yyjson_mut_val* mk_invoke(yyjson_mut_doc* doc, yyjson_mut_val* node, const c8* handler, const c8* target) {
  yyjson_mut_val* on = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_val(doc, node, "on", on);
  yyjson_mut_obj_add_strcpy(doc, on, "action", "invoke");
  yyjson_mut_obj_add_strcpy(doc, on, "event", "click");
  yyjson_mut_obj_add_strcpy(doc, on, "handler", handler);
  yyjson_mut_obj_add_strcpy(doc, on, "onResponse", "patch");
  yyjson_mut_obj_add_strcpy(doc, on, "target", target);
  yyjson_mut_val* body = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_val(doc, on, "body", body);
  return body;
}

static void mk_body_arg(yyjson_mut_doc* doc, yyjson_mut_val* body, const c8* key, sp_str_t value) {
  yyjson_mut_obj_add_strncpy(doc, body, key, value.data, value.len);
}

static sp_str_t quote_ident(sp_mem_t mem, sp_str_t name) {
  sp_da(c8) out = sp_da_new(mem, c8);
  sp_da_push(out, '"');
  sp_for(i, name.len) {
    if (name.data[i] == '"') sp_da_push(out, '"');
    sp_da_push(out, name.data[i]);
  }
  sp_da_push(out, '"');
  return sp_str(out, (u32)sp_da_size(out));
}

static sp_str_t column_value(sp_mem_t mem, sqlite3_stmt* stmt, s32 col) {
  if (sqlite3_column_type(stmt, col) == SQLITE_NULL) return sp_str_lit("∅");
  const c8* value = (const c8*)sqlite3_column_text(stmt, col);
  return sp_str_copy(mem, sp_cstr_as_str(value ? value : ""));
}

static bool table_exists(demo_ctx_t* app, sp_str_t table) {
  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(app->db, "select 1 from sqlite_master where type = 'table' and name = ?", -1, &stmt, SP_NULLPTR) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, table.data, (s32)table.len, SQLITE_TRANSIENT);
  bool found = sqlite3_step(stmt) == SQLITE_ROW;
  sqlite3_finalize(stmt);
  return found;
}

static spry_reply_t ep_tables(void* ctx, const spry_args_t* args) {
  (void)args;
  demo_ctx_t* app = ctx;

  yyjson_mut_doc* doc = yyjson_mut_doc_new(SP_NULLPTR);
  yyjson_mut_val* root = mk_box(doc, "column", 6);
  yyjson_mut_doc_set_root(doc, root);
  yyjson_mut_val* children = mk_children(doc, root);
  yyjson_mut_arr_add_val(children, mk_text(doc, sp_str_lit("tables")));

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(app->db, "select name from sqlite_master where type = 'table' and name not like 'sqlite_%' order by name", -1, &stmt, SP_NULLPTR) != SQLITE_OK) {
    yyjson_mut_doc_free(doc);
    return spry_fault(app->rpc, "failed", sp_cstr_as_str(sqlite3_errmsg(app->db)));
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    sp_str_t name = column_value(app->mem, stmt, 0);
    yyjson_mut_val* btn = mk_button(doc, name);
    yyjson_mut_val* body = mk_invoke(doc, btn, "open_table", "grid");
    mk_body_arg(doc, body, "table", name);
    yyjson_mut_arr_add_val(children, btn);
  }
  sqlite3_finalize(stmt);

  return spry_ok_doc(app->rpc, doc);
}

static spry_reply_t grid_reply(demo_ctx_t* app, sp_str_t table) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(app->mem);
  sp_str_t sql = sp_fmt(scratch.mem, "select rowid as _spry_rowid, * from {} limit {}", sp_fmt_str(quote_ident(scratch.mem, table)), sp_fmt_int(ROW_LIMIT)).value;
  sqlite3_stmt* stmt;
  s32 prc = sqlite3_prepare_v2(app->db, sql.data, (s32)sql.len, &stmt, SP_NULLPTR);
  sp_mem_end_scratch(scratch);
  if (prc != SQLITE_OK) {
    return spry_fault(app->rpc, "failed", sp_cstr_as_str(sqlite3_errmsg(app->db)));
  }

  yyjson_mut_doc* doc = yyjson_mut_doc_new(SP_NULLPTR);
  yyjson_mut_val* root = mk_box(doc, "column", 4);
  yyjson_mut_doc_set_root(doc, root);
  yyjson_mut_val* children = mk_children(doc, root);

  sp_str_t title = sp_fmt(app->mem, "{} (first {} rows; click a cell to edit)", sp_fmt_str(table), sp_fmt_int(ROW_LIMIT)).value;
  yyjson_mut_arr_add_val(children, mk_text(doc, title));

  s32 ncols = sqlite3_column_count(stmt);
  yyjson_mut_val* header = mk_box(doc, "row", 16);
  yyjson_mut_val* header_children = mk_children(doc, header);
  sp_for_range(col, 1, ncols) {
    yyjson_mut_arr_add_val(header_children, mk_text(doc, sp_cstr_as_str(sqlite3_column_name(stmt, (s32)col))));
  }
  yyjson_mut_arr_add_val(children, header);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    sp_str_t rowid = column_value(app->mem, stmt, 0);
    yyjson_mut_val* data_row = mk_box(doc, "row", 16);
    yyjson_mut_val* row_children = mk_children(doc, data_row);
    sp_for_range(col, 1, ncols) {
      yyjson_mut_val* cell = mk_button(doc, column_value(app->mem, stmt, (s32)col));
      yyjson_mut_val* body = mk_invoke(doc, cell, "edit_cell", "editor");
      mk_body_arg(doc, body, "table", table);
      mk_body_arg(doc, body, "rowid", rowid);
      mk_body_arg(doc, body, "column", sp_cstr_as_str(sqlite3_column_name(stmt, (s32)col)));
      yyjson_mut_arr_add_val(row_children, cell);
    }
    yyjson_mut_arr_add_val(children, data_row);
  }
  sqlite3_finalize(stmt);

  return spry_ok_doc(app->rpc, doc);
}

static spry_reply_t ep_open_table(void* ctx, const spry_args_t* args) {
  demo_ctx_t* app = ctx;
  sp_str_t table;
  if (!spry_args_str(args, "table", &table)) return spry_fault(app->rpc, "invalid", sp_str_lit("table is required"));
  if (!table_exists(app, table)) return spry_fault(app->rpc, "missing", sp_fmt(app->mem, "no such table '{}'", sp_fmt_str(table)).value);
  return grid_reply(app, table);
}

static spry_reply_t ep_edit_cell(void* ctx, const spry_args_t* args) {
  demo_ctx_t* app = ctx;
  sp_str_t table, col;
  s64 rowid;
  if (!spry_args_str(args, "table", &table) || !spry_args_s64(args, "rowid", &rowid) || !spry_args_str(args, "column", &col)) {
    return spry_fault(app->rpc, "invalid", sp_str_lit("table, rowid, column are required"));
  }
  if (!table_exists(app, table)) return spry_fault(app->rpc, "missing", sp_fmt(app->mem, "no such table '{}'", sp_fmt_str(table)).value);

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(app->mem);
  sp_str_t sql = sp_fmt(scratch.mem, "select {} from {} where rowid = ?", sp_fmt_str(quote_ident(scratch.mem, col)), sp_fmt_str(quote_ident(scratch.mem, table))).value;
  sqlite3_stmt* stmt;
  s32 prc = sqlite3_prepare_v2(app->db, sql.data, (s32)sql.len, &stmt, SP_NULLPTR);
  sp_mem_end_scratch(scratch);
  if (prc != SQLITE_OK) {
    return spry_fault(app->rpc, "missing", sp_cstr_as_str(sqlite3_errmsg(app->db)));
  }
  sqlite3_bind_int64(stmt, 1, rowid);
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return spry_fault(app->rpc, "missing", sp_fmt(app->mem, "no row {} in '{}'", sp_fmt_int(rowid), sp_fmt_str(table)).value);
  }
  sp_str_t current = column_value(app->mem, stmt, 0);
  sqlite3_finalize(stmt);

  yyjson_mut_doc* doc = yyjson_mut_doc_new(SP_NULLPTR);
  yyjson_mut_val* root = mk_box(doc, "column", 8);
  yyjson_mut_doc_set_root(doc, root);
  yyjson_mut_val* children = mk_children(doc, root);

  sp_str_t heading = sp_fmt(app->mem, "editing {}[{}].{}", sp_fmt_str(table), sp_fmt_int(rowid), sp_fmt_str(col)).value;
  yyjson_mut_arr_add_val(children, mk_text(doc, heading));

  yyjson_mut_val* form = mk_box(doc, "row", 8);
  yyjson_mut_val* form_children = mk_children(doc, form);
  yyjson_mut_arr_add_val(form_children, mk_input(doc, "value", current));

  yyjson_mut_val* save = mk_button(doc, sp_str_lit("Save"));
  yyjson_mut_val* body = mk_invoke(doc, save, "save_cell", "grid");
  mk_body_arg(doc, body, "table", table);
  mk_body_arg(doc, body, "rowid", sp_fmt(app->mem, "{}", sp_fmt_int(rowid)).value);
  mk_body_arg(doc, body, "column", col);
  yyjson_mut_arr_add_val(form_children, save);

  yyjson_mut_arr_add_val(children, form);
  return spry_ok_doc(app->rpc, doc);
}

static spry_reply_t ep_save_cell(void* ctx, const spry_args_t* args) {
  demo_ctx_t* app = ctx;
  sp_str_t table, col, value;
  s64 rowid;
  if (!spry_args_str(args, "table", &table) || !spry_args_s64(args, "rowid", &rowid) ||
      !spry_args_str(args, "column", &col) || !spry_args_str(args, "value", &value)) {
    return spry_fault(app->rpc, "invalid", sp_str_lit("table, rowid, column, value are required"));
  }
  if (!table_exists(app, table)) return spry_fault(app->rpc, "missing", sp_fmt(app->mem, "no such table '{}'", sp_fmt_str(table)).value);

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(app->mem);
  sp_str_t sql = sp_fmt(scratch.mem, "update {} set {} = ? where rowid = ?", sp_fmt_str(quote_ident(scratch.mem, table)), sp_fmt_str(quote_ident(scratch.mem, col))).value;
  sqlite3_stmt* stmt;
  s32 prc = sqlite3_prepare_v2(app->db, sql.data, (s32)sql.len, &stmt, SP_NULLPTR);
  sp_mem_end_scratch(scratch);
  if (prc != SQLITE_OK) {
    return spry_fault(app->rpc, "missing", sp_cstr_as_str(sqlite3_errmsg(app->db)));
  }
  sqlite3_bind_text(stmt, 1, value.data, (s32)value.len, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, rowid);
  s32 rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    return spry_fault(app->rpc, "failed", sp_cstr_as_str(sqlite3_errmsg(app->db)));
  }

  return grid_reply(app, table);
}

static spry_reply_t ep_exec(void* ctx, const spry_args_t* args) {
  demo_ctx_t* app = ctx;
  sp_str_t sql;
  if (!spry_args_str(args, "sql", &sql)) return spry_fault(app->rpc, "invalid", sp_str_lit("sql is required"));

  yyjson_mut_doc* doc = yyjson_mut_doc_new(SP_NULLPTR);
  yyjson_mut_val* root = mk_box(doc, "column", 4);
  yyjson_mut_doc_set_root(doc, root);
  yyjson_mut_val* children = mk_children(doc, root);

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(app->db, sql.data, (s32)sql.len, &stmt, SP_NULLPTR) != SQLITE_OK) {
    sp_str_t message = sp_fmt(app->mem, "SQL error: {}", sp_fmt_cstr(sqlite3_errmsg(app->db))).value;
    yyjson_mut_arr_add_val(children, mk_text(doc, message));
    return spry_ok_doc(app->rpc, doc);
  }

  s32 ncols = sqlite3_column_count(stmt);
  if (ncols > 0) {
    yyjson_mut_val* header = mk_box(doc, "row", 16);
    yyjson_mut_val* header_children = mk_children(doc, header);
    sp_for(col, ncols) {
      yyjson_mut_arr_add_val(header_children, mk_text(doc, sp_cstr_as_str(sqlite3_column_name(stmt, (s32)col))));
    }
    yyjson_mut_arr_add_val(children, header);
  }

  s32 rows = 0;
  s32 rc;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && rows < ROW_LIMIT) {
    yyjson_mut_val* data_row = mk_box(doc, "row", 16);
    yyjson_mut_val* row_children = mk_children(doc, data_row);
    sp_for(col, ncols) {
      yyjson_mut_arr_add_val(row_children, mk_text(doc, column_value(app->mem, stmt, (s32)col)));
    }
    yyjson_mut_arr_add_val(children, data_row);
    rows += 1;
  }

  if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
    sp_str_t message = sp_fmt(app->mem, "SQL error: {}", sp_fmt_cstr(sqlite3_errmsg(app->db))).value;
    yyjson_mut_arr_add_val(children, mk_text(doc, message));
  } else if (rows == 0) {
    yyjson_mut_arr_add_val(children, mk_text(doc, sp_str_lit("(no rows)")));
  }
  sqlite3_finalize(stmt);

  return spry_ok_doc(app->rpc, doc);
}

static const c8* SEED_SQL =
  "create table if not exists artists (id integer primary key, name text not null);"
  "create table if not exists albums (id integer primary key, title text not null, artist_id integer references artists(id), year integer);"
  "insert into artists (name) select * from (values ('Can'), ('Neu!'), ('Faust'), ('Cluster')) where (select count(*) from artists) = 0;"
  "insert into albums (title, artist_id, year) select * from (values ('Tago Mago', 1, 1971), ('Ege Bamyasi', 1, 1972), ('Future Days', 1, 1973), ('Neu!', 2, 1972), ('Neu! 75', 2, 1975), ('Faust IV', 3, 1973), ('Zuckerzeit', 4, 1974)) where (select count(*) from albums) = 0;";

bool demo_db_seed(sqlite3* db) {
  c8* err = SP_NULLPTR;
  return sqlite3_exec(db, SEED_SQL, SP_NULLPTR, SP_NULLPTR, &err) == SQLITE_OK;
}

demo_ctx_t* demo_new(sp_mem_t mem, sqlite3* db) {
  demo_ctx_t* ctx = sp_alloc_type(mem, demo_ctx_t);
  *ctx = sp_zero_s(demo_ctx_t);
  ctx->mem = mem;
  ctx->db = db;
  ctx->rpc = spry_rpc_new(mem);
  spry_rpc_register(ctx->rpc, sp_str_lit("tables"), ep_tables, ctx);
  spry_rpc_register(ctx->rpc, sp_str_lit("open_table"), ep_open_table, ctx);
  spry_rpc_register(ctx->rpc, sp_str_lit("edit_cell"), ep_edit_cell, ctx);
  spry_rpc_register(ctx->rpc, sp_str_lit("save_cell"), ep_save_cell, ctx);
  spry_rpc_register(ctx->rpc, sp_str_lit("exec"), ep_exec, ctx);
  return ctx;
}
