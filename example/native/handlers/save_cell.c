#include "demo.endpoints.gen.h"
#include "app/app.h"
#include "db/db.h"

spry_reply_t ep_save_cell(void* ctx, const demo_save_cell_args_t* args) {
  demo_ctx_t* app = ctx;
  sp_mem_t mem = spry_request_mem(app->rpc);

  if (!demo_db_table_exists(app->db, args->table)) {
    return missing_table(app, args->table);
  }
  if (demo_db_update_cell(app->db, mem, args->table, args->column, args->rowid, args->value)) {
    return spry_fault(app->rpc, SPRY_CODE_FAILED, demo_db_error(app->db));
  }
  return render_table_as_grid(app, args->table);
}

