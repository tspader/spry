#include "sp.h"
#include "spry/rpc.h"
#include "spry/ui.h"
#include "demo.endpoints.gen.h"
#include "app/app.h"
#include "db/db.h"

demo_ctx_t* demo_new(sp_mem_t mem, sqlite3* db, spry_endpoints_t endpoints) {
  demo_ctx_t* app = sp_alloc_type(mem, demo_ctx_t);
  *app = sp_zero_s(demo_ctx_t);
  app->db = db;
  app->rpc = spry_rpc_new(mem, endpoints);
  demo_register(app->rpc, (demo_handlers_t){
    .tables = ep_tables,
    .open_table = ep_open_table,
    .edit_cell = ep_edit_cell,
    .save_cell = ep_save_cell,
    .exec = ep_exec,
  }, app);
  return app;
}

spry_reply_t missing_table(demo_ctx_t* app, sp_str_t table) {
  return spry_fault_fmt(app->rpc, SPRY_CODE_MISSING, "no such table {.quote}", sp_fmt_str(table));
}

spry_reply_t render_table_as_grid(demo_ctx_t* app, sp_str_t table) {
  demo_db_grid_t grid;
  if (demo_db_grid(app->db, spry_request_mem(app->rpc), table, &grid)) {
    return spry_fault(app->rpc, SPRY_CODE_FAILED, demo_db_error(app->db));
  }

  spry_ui_t* ui = spry_ui_begin(app->rpc);
  SPRY_UI(ui) {
    SPRY_COLUMN(.gap = 4) {
      SPRY_TEXTF("{} (first {} rows; click a cell to edit)", sp_fmt_str(table), sp_fmt_int(DEMO_ROW_LIMIT));

      SPRY_ROW(.gap = 16) {
        sp_da_for(grid.columns, col) SPRY_TEXT(grid.columns[col]);
      }

      sp_da_for(grid.rows, row) {
        SPRY_ROW(.gap = 16) {
          sp_da_for(grid.columns, col) {
            SPRY_BUTTON(grid.rows[row].cells[col], .on = SPRY_PATCH("edit_cell", "editor")) {
              SPRY_ARG("table", table);
              SPRY_ARG("rowid", grid.rows[row].rowid);
              SPRY_ARG("column", grid.columns[col]);
            }
          }
        }
      }
    }
  }
  return spry_ok_ui(ui);
}

