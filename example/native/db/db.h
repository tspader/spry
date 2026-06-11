#ifndef DEMO_DB_H
#define DEMO_DB_H

#include <sqlite3.h>
#include "sp.h"

#define DEMO_ROW_LIMIT 20

typedef enum {
  DEMO_DB_OK,
  DEMO_DB_ERROR,
  DEMO_DB_MISSING,
} demo_db_err_t;

typedef sp_da(sp_str_t) demo_strs_t;

typedef struct {
  s64 rowid;
  demo_strs_t cells;
} demo_db_row_t;

typedef struct {
  demo_strs_t columns;
  sp_da(demo_db_row_t) rows;
} demo_db_grid_t;

typedef struct {
  demo_strs_t columns;
  sp_da(demo_strs_t) rows;
} demo_db_table_t;

demo_db_err_t demo_db_open(sqlite3** out);
bool          demo_db_table_exists(sqlite3* db, sp_str_t table);
demo_db_err_t demo_db_tables(sqlite3* db, sp_mem_t mem, demo_strs_t* out);
demo_db_err_t demo_db_grid(sqlite3* db, sp_mem_t mem, sp_str_t table, demo_db_grid_t* out);
demo_db_err_t demo_db_cell(sqlite3* db, sp_mem_t mem, sp_str_t table, sp_str_t column, s32 rowid, sp_str_t* out);
demo_db_err_t demo_db_update_cell(sqlite3* db, sp_mem_t mem, sp_str_t table, sp_str_t column, s32 rowid, sp_str_t value);
demo_db_err_t demo_db_exec(sp_mem_t mem, sqlite3* db, sp_str_t sql, demo_db_table_t* out);
sp_str_t      demo_db_error(sqlite3* db);

#endif
