#include "demo.endpoints.gen.h"
#include "app/app.h"
#include "db/db.h"

spry_reply_t ep_edit_cell(void* ctx, const demo_edit_cell_args_t* args) {
  demo_ctx_t* app = ctx;
  if (!demo_db_table_exists(app->db, args->table)) {
    return missing_table(app, args->table);
  }

  sp_str_t current;
  switch (demo_db_cell(app->db, spry_request_mem(app->rpc), args->table, args->column, args->rowid, &current)) {
    case DEMO_DB_OK: break;
    case DEMO_DB_ERROR: return spry_fault(app->rpc, SPRY_CODE_FAILED, demo_db_error(app->db));
    case DEMO_DB_MISSING: return spry_fault_fmt(app->rpc, SPRY_CODE_MISSING, "no row {} in {.quote}", sp_fmt_int(args->rowid), sp_fmt_str(args->table));
  }

  spry_ui_t* ui = spry_ui_begin(app->rpc);
  SPRY_UI(ui) {
    SPRY_COLUMN(.gap = 8) {
      SPRY_TEXTF("editing {}[{}].{}", sp_fmt_str(args->table), sp_fmt_int(args->rowid), sp_fmt_str(args->column));
      SPRY_ROW(.gap = 8, .align = SPRY_ALIGN_CENTER) {
        SPRY_INPUT("value", .value = current);
        SPRY_BUTTON("Save", .on = SPRY_PATCH("save_cell", "grid")) {
          SPRY_ARG("table", args->table);
          SPRY_ARG("rowid", args->rowid);
          SPRY_ARG("column", args->column);
        }
      }
    }
  }
  return spry_ok_ui(ui);
}

