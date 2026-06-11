#include <sqlite3.h>
#include "sp.h"
#include "spry/rpc.h"
#include "spry/ui.h"
#include "demo.endpoints.gen.h"
#include "handlers.h"

#define ROW_LIMIT 20

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

static spry_reply_t ep_tables(void* ctx) {
  demo_ctx_t* app = ctx;

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(app->db, "select name from sqlite_master where type = 'table' and name not like 'sqlite_%' order by name", -1, &stmt, SP_NULLPTR) != SQLITE_OK) {
    return spry_fault(app->rpc, SPRY_CODE_FAILED, sp_cstr_as_str(sqlite3_errmsg(app->db)));
  }

  spry_ui_t* ui = spry_ui_new(app->mem);
  SPRY_UI(ui) {
    SPRY_COLUMN(.gap = 6) {
      SPRY_TEXT("tables");
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        sp_str_t name = column_value(app->mem, stmt, 0);
        SPRY_BUTTON(name, .on = SPRY_PATCH("open_table", "grid")) {
          SPRY_ARG("table", name);
        }
      }
    }
  }
  sqlite3_finalize(stmt);

  return spry_ok_ui(app->rpc, ui);
}

static spry_reply_t grid_reply(demo_ctx_t* app, sp_str_t table) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(app->mem);
  sp_str_t sql = sp_fmt(scratch.mem, "select rowid as _spry_rowid, * from {} limit {}", sp_fmt_str(quote_ident(scratch.mem, table)), sp_fmt_int(ROW_LIMIT)).value;
  sqlite3_stmt* stmt;
  s32 prc = sqlite3_prepare_v2(app->db, sql.data, (s32)sql.len, &stmt, SP_NULLPTR);
  sp_mem_end_scratch(scratch);
  if (prc != SQLITE_OK) {
    return spry_fault(app->rpc, SPRY_CODE_FAILED, sp_cstr_as_str(sqlite3_errmsg(app->db)));
  }

  spry_ui_t* ui = spry_ui_new(app->mem);
  SPRY_UI(ui) {
    SPRY_COLUMN(.gap = 4) {
      SPRY_TEXTF("{} (first {} rows; click a cell to edit)", sp_fmt_str(table), sp_fmt_int(ROW_LIMIT));

      s32 ncols = sqlite3_column_count(stmt);
      SPRY_ROW(.gap = 16) {
        sp_for_range(col, 1, ncols) {
          SPRY_TEXT(sp_str_copy(app->mem, sp_cstr_as_str(sqlite3_column_name(stmt, (s32)col))));
        }
      }

      while (sqlite3_step(stmt) == SQLITE_ROW) {
        sp_str_t rowid = column_value(app->mem, stmt, 0);
        SPRY_ROW(.gap = 16) {
          sp_for_range(col, 1, ncols) {
            sp_str_t name = sp_str_copy(app->mem, sp_cstr_as_str(sqlite3_column_name(stmt, (s32)col)));
            SPRY_BUTTON(column_value(app->mem, stmt, (s32)col), .on = SPRY_PATCH("edit_cell", "editor")) {
              SPRY_ARG("table", table);
              SPRY_ARG("rowid", rowid);
              SPRY_ARG("column", name);
            }
          }
        }
      }
    }
  }
  sqlite3_finalize(stmt);

  return spry_ok_ui(app->rpc, ui);
}

static spry_reply_t ep_open_table(void* ctx, const demo_open_table_args_t* args) {
  demo_ctx_t* app = ctx;
  if (!table_exists(app, args->table)) return spry_fault(app->rpc, SPRY_CODE_MISSING, sp_fmt(app->mem, "no such table '{}'", sp_fmt_str(args->table)).value);
  return grid_reply(app, args->table);
}

static spry_reply_t ep_edit_cell(void* ctx, const demo_edit_cell_args_t* args) {
  demo_ctx_t* app = ctx;
  sp_str_t table = args->table;
  s32 rowid = args->rowid;
  sp_str_t col = args->column;
  if (!table_exists(app, table)) return spry_fault(app->rpc, SPRY_CODE_MISSING, sp_fmt(app->mem, "no such table '{}'", sp_fmt_str(table)).value);

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(app->mem);
  sp_str_t sql = sp_fmt(scratch.mem, "select {} from {} where rowid = ?", sp_fmt_str(quote_ident(scratch.mem, col)), sp_fmt_str(quote_ident(scratch.mem, table))).value;
  sqlite3_stmt* stmt;
  s32 prc = sqlite3_prepare_v2(app->db, sql.data, (s32)sql.len, &stmt, SP_NULLPTR);
  sp_mem_end_scratch(scratch);
  if (prc != SQLITE_OK) {
    return spry_fault(app->rpc, SPRY_CODE_MISSING, sp_cstr_as_str(sqlite3_errmsg(app->db)));
  }
  sqlite3_bind_int64(stmt, 1, rowid);
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return spry_fault(app->rpc, SPRY_CODE_MISSING, sp_fmt(app->mem, "no row {} in '{}'", sp_fmt_int(rowid), sp_fmt_str(table)).value);
  }
  sp_str_t current = column_value(app->mem, stmt, 0);
  sqlite3_finalize(stmt);

  spry_ui_t* ui = spry_ui_new(app->mem);
  SPRY_UI(ui) {
    SPRY_COLUMN(.gap = 8) {
      SPRY_TEXTF("editing {}[{}].{}", sp_fmt_str(table), sp_fmt_int(rowid), sp_fmt_str(col));
      SPRY_ROW(.gap = 8, .align = SPRY_ALIGN_CENTER) {
        SPRY_INPUT(.name = sp_str_lit("value"), .value = current);
        SPRY_BUTTON("Save", .on = SPRY_PATCH("save_cell", "grid")) {
          SPRY_ARG("table", table);
          SPRY_ARG("rowid", sp_fmt(app->mem, "{}", sp_fmt_int(rowid)).value);
          SPRY_ARG("column", col);
        }
      }
    }
  }
  return spry_ok_ui(app->rpc, ui);
}

static spry_reply_t ep_save_cell(void* ctx, const demo_save_cell_args_t* args) {
  demo_ctx_t* app = ctx;
  sp_str_t table = args->table;
  s32 rowid = args->rowid;
  sp_str_t col = args->column;
  sp_str_t value = args->value;
  if (!table_exists(app, table)) return spry_fault(app->rpc, SPRY_CODE_MISSING, sp_fmt(app->mem, "no such table '{}'", sp_fmt_str(table)).value);

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(app->mem);
  sp_str_t sql = sp_fmt(scratch.mem, "update {} set {} = ? where rowid = ?", sp_fmt_str(quote_ident(scratch.mem, table)), sp_fmt_str(quote_ident(scratch.mem, col))).value;
  sqlite3_stmt* stmt;
  s32 prc = sqlite3_prepare_v2(app->db, sql.data, (s32)sql.len, &stmt, SP_NULLPTR);
  sp_mem_end_scratch(scratch);
  if (prc != SQLITE_OK) {
    return spry_fault(app->rpc, SPRY_CODE_MISSING, sp_cstr_as_str(sqlite3_errmsg(app->db)));
  }
  sqlite3_bind_text(stmt, 1, value.data, (s32)value.len, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, rowid);
  s32 rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    return spry_fault(app->rpc, SPRY_CODE_FAILED, sp_cstr_as_str(sqlite3_errmsg(app->db)));
  }

  return grid_reply(app, table);
}

static spry_reply_t ep_exec(void* ctx, const demo_exec_args_t* args) {
  demo_ctx_t* app = ctx;
  sp_str_t sql = args->sql;

  spry_ui_t* ui = spry_ui_new(app->mem);

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(app->db, sql.data, (s32)sql.len, &stmt, SP_NULLPTR) != SQLITE_OK) {
    SPRY_UI(ui) {
      SPRY_COLUMN(.gap = 4) {
        SPRY_TEXTF("SQL error: {}", sp_fmt_cstr(sqlite3_errmsg(app->db)));
      }
    }
    return spry_ok_ui(app->rpc, ui);
  }

  s32 rc = SQLITE_DONE;
  SPRY_UI(ui) {
    SPRY_COLUMN(.gap = 4) {
      s32 ncols = sqlite3_column_count(stmt);
      if (ncols > 0) {
        SPRY_ROW(.gap = 16) {
          sp_for(col, ncols) {
            SPRY_TEXT(sp_str_copy(app->mem, sp_cstr_as_str(sqlite3_column_name(stmt, (s32)col))));
          }
        }
      }

      s32 rows = 0;
      while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && rows < ROW_LIMIT) {
        SPRY_ROW(.gap = 16) {
          sp_for(col, ncols) {
            SPRY_TEXT(column_value(app->mem, stmt, (s32)col));
          }
        }
        rows += 1;
      }

      if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        SPRY_TEXTF("SQL error: {}", sp_fmt_cstr(sqlite3_errmsg(app->db)));
      } else if (rows == 0) {
        SPRY_TEXT("(no rows)");
      }
    }
  }
  sqlite3_finalize(stmt);

  return spry_ok_ui(app->rpc, ui);
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

demo_ctx_t* demo_new(sp_mem_t mem, sqlite3* db, spry_endpoints_t endpoints) {
  demo_ctx_t* ctx = sp_alloc_type(mem, demo_ctx_t);
  *ctx = sp_zero_s(demo_ctx_t);
  ctx->mem = mem;
  ctx->db = db;
  ctx->rpc = spry_rpc_new(mem, endpoints);
  demo_register_tables(ctx->rpc, ep_tables, ctx);
  demo_register_open_table(ctx->rpc, ep_open_table, ctx);
  demo_register_edit_cell(ctx->rpc, ep_edit_cell, ctx);
  demo_register_save_cell(ctx->rpc, ep_save_cell, ctx);
  demo_register_exec(ctx->rpc, ep_exec, ctx);
  return ctx;
}
