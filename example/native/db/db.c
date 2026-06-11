#include "db.h"
#include "sp.h"
#include "sql.h"

static sp_str_t db_quote(sp_mem_t mem, sp_str_t name) {
  sp_da(c8) out = sp_da_new(mem, c8);
  sp_da_push(out, '"');
  sp_for(i, name.len) {
    if (name.data[i] == '"') sp_da_push(out, '"');
    sp_da_push(out, name.data[i]);
  }
  sp_da_push(out, '"');
  return sp_str(out, (u32)sp_da_size(out));
}

static sp_str_t db_text_raw(sp_mem_t mem, sqlite3_stmt* stmt, s32 col) {
  const c8* value = (const c8*)sqlite3_column_text(stmt, col);
  return sp_str_copy(mem, sp_cstr_as_str(value ? value : ""));
}

static sp_str_t db_text(sp_mem_t mem, sqlite3_stmt* stmt, s32 col) {
  if (sqlite3_column_type(stmt, col) == SQLITE_NULL) return sp_str_lit("∅");
  return db_text_raw(mem, stmt, col);
}

static sp_str_t db_column_name(sp_mem_t mem, sqlite3_stmt* stmt, s32 col) {
  return sp_str_copy(mem, sp_cstr_as_str(sqlite3_column_name(stmt, col)));
}

static bool db_prepare(sqlite3* db, sp_str_t sql, sqlite3_stmt** out) {
  return sqlite3_prepare_v2(db, sql.data, (s32)sql.len, out, SP_NULLPTR) == SQLITE_OK;
}

demo_db_err_t demo_db_open(sqlite3** out) {
  // Always a fresh in-memory db, reseeded on every startup; never touches disk.
  if (sqlite3_open(":memory:", out) != SQLITE_OK) return DEMO_DB_ERROR;
  if (sqlite3_exec(*out, DEMO_SQL_SEED, SP_NULLPTR, SP_NULLPTR, SP_NULLPTR) != SQLITE_OK) return DEMO_DB_ERROR;
  return DEMO_DB_OK;
}

sp_str_t demo_db_error(sqlite3* db) {
  return sp_cstr_as_str(sqlite3_errmsg(db));
}

bool demo_db_table_exists(sqlite3* db, sp_str_t table) {
  sqlite3_stmt* stmt;
  if (!db_prepare(db, sp_str_lit(DEMO_SQL_TABLE_EXISTS), &stmt)) return false;
  sqlite3_bind_text(stmt, 1, table.data, (s32)table.len, SQLITE_TRANSIENT);
  bool found = sqlite3_step(stmt) == SQLITE_ROW;
  sqlite3_finalize(stmt);
  return found;
}

demo_db_err_t demo_db_tables(sqlite3* db, sp_mem_t mem, demo_strs_t* out) {
  sqlite3_stmt* stmt;
  if (!db_prepare(db, sp_str_lit(DEMO_SQL_TABLES), &stmt)) return DEMO_DB_ERROR;
  *out = sp_da_new(mem, sp_str_t);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    sp_da_push(*out, db_text(mem, stmt, 0));
  }
  sqlite3_finalize(stmt);
  return DEMO_DB_OK;
}

demo_db_err_t demo_db_grid(sqlite3* db, sp_mem_t mem, sp_str_t table, demo_db_grid_t* out) {
  sp_str_t sql = sp_fmt(mem, DEMO_SQL_GRID, sp_fmt_str(db_quote(mem, table)), sp_fmt_int(DEMO_ROW_LIMIT)).value;
  sqlite3_stmt* stmt;
  if (!db_prepare(db, sql, &stmt)) return DEMO_DB_ERROR;

  *out = (demo_db_grid_t){
    .columns = sp_da_new(mem, sp_str_t),
    .rows = sp_da_new(mem, demo_db_row_t),
  };

  s32 ncols = sqlite3_column_count(stmt);
  sp_for_range(col, 1, (u32)ncols) {
    sp_da_push(out->columns, db_column_name(mem, stmt, (s32)col));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    demo_db_row_t row = {
      .rowid = sqlite3_column_int64(stmt, 0),
      .cells = sp_da_new(mem, sp_str_t),
    };
    sp_for_range(col, 1, (u32)ncols) {
      sp_da_push(row.cells, db_text(mem, stmt, (s32)col));
    }
    sp_da_push(out->rows, row);
  }
  sqlite3_finalize(stmt);
  return DEMO_DB_OK;
}

demo_db_err_t demo_db_cell(sqlite3* db, sp_mem_t mem, sp_str_t table, sp_str_t column, s32 rowid, sp_str_t* out) {
  sp_str_t sql = sp_fmt(mem, DEMO_SQL_CELL, sp_fmt_str(db_quote(mem, column)), sp_fmt_str(db_quote(mem, table))).value;
  sqlite3_stmt* stmt;
  if (!db_prepare(db, sql, &stmt)) return DEMO_DB_ERROR;
  sqlite3_bind_int64(stmt, 1, rowid);
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return DEMO_DB_MISSING;
  }
  *out = db_text_raw(mem, stmt, 0);
  sqlite3_finalize(stmt);
  return DEMO_DB_OK;
}

demo_db_err_t demo_db_update_cell(sqlite3* db, sp_mem_t mem, sp_str_t table, sp_str_t column, s32 rowid, sp_str_t value) {
  sp_str_t sql = sp_fmt(mem, DEMO_SQL_UPDATE_CELL, sp_fmt_str(db_quote(mem, table)), sp_fmt_str(db_quote(mem, column))).value;
  sqlite3_stmt* stmt;
  if (!db_prepare(db, sql, &stmt)) return DEMO_DB_ERROR;

  sqlite3_bind_text(stmt, 1, value.data, (s32)value.len, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, rowid);
  s32 rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE ? DEMO_DB_OK : DEMO_DB_ERROR;
}

demo_db_err_t demo_db_exec(sp_mem_t mem, sqlite3* db, sp_str_t sql, demo_db_table_t* out) {
  sqlite3_stmt* stmt;
  if (!db_prepare(db, sql, &stmt)) return DEMO_DB_ERROR;

  *out = (demo_db_table_t){
    .columns = sp_da_new(mem, sp_str_t),
    .rows = sp_da_new(mem, demo_strs_t),
  };

  s32 ncols = sqlite3_column_count(stmt);
  sp_for(col, (u32)ncols) {
    sp_da_push(out->columns, db_column_name(mem, stmt, (s32)col));
  }

  s32 rc;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && sp_da_size(out->rows) < DEMO_ROW_LIMIT) {
    demo_strs_t row = sp_da_new(mem, sp_str_t);
    sp_for(col, (u32)ncols) {
      sp_da_push(row, db_text(mem, stmt, (s32)col));
    }
    sp_da_push(out->rows, row);
  }
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE || rc == SQLITE_ROW ? DEMO_DB_OK : DEMO_DB_ERROR;
}
