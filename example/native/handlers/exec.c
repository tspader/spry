#include "demo.endpoints.gen.h"
#include "app/app.h"
#include "db/db.h"

spry_reply_t ep_exec(void* ctx, const demo_exec_args_t* args) {
  demo_ctx_t* app = ctx;
  sp_mem_t mem = spry_request_mem(app->rpc);
  spry_ui_t* ui = spry_ui_begin(app->rpc);

  demo_db_table_t result;
  if (demo_db_exec(mem, app->db, args->sql, &result)) {
    SPRY_UI(ui) {
      SPRY_TEXTF("SQL error: {}", sp_fmt_str(demo_db_error(app->db)));
    }
    return spry_ok_ui(ui);
  }

  SPRY_UI(ui) {
    SPRY_COLUMN(.gap = 4) {
      if (sp_da_size(result.columns)) {
        SPRY_ROW(.gap = 16) {
          sp_da_for(result.columns, col) SPRY_TEXT(result.columns[col]);
        }
      }

      sp_da_for(result.rows, row) {
        SPRY_ROW(.gap = 16) {
          sp_da_for(result.rows[row], col) SPRY_TEXT(result.rows[row][col]);
        }
      }

      if (!sp_da_size(result.rows)) SPRY_TEXT("(no rows)");
    }
  }
  return spry_ok_ui(ui);
}
