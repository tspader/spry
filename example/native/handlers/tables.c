#include "app/app.h"
#include "db/db.h"

spry_reply_t ep_tables(void* ctx) {
  demo_ctx_t* app = ctx;

  demo_strs_t tables;
  if (demo_db_tables(app->db, spry_request_mem(app->rpc), &tables)) {
    return spry_fault(app->rpc, SPRY_CODE_FAILED, demo_db_error(app->db));
  }

  spry_ui_t* ui = spry_ui_begin(app->rpc);
  SPRY_UI(ui) {
    SPRY_COLUMN(.gap = 6) {
      SPRY_TEXT("tables");
      sp_da_for(tables, i) {
        SPRY_BUTTON(tables[i], .on = SPRY_PATCH("open_table", "grid")) {
          SPRY_ARG("table", tables[i]);
        }
      }
    }
  }
  return spry_ok_ui(ui);
}
