#include "demo.endpoints.gen.h"
#include "app/app.h"
#include "db/db.h"

spry_reply_t ep_open_table(void* ctx, const demo_open_table_args_t* args) {
  demo_ctx_t* app = ctx;
  if (!demo_db_table_exists(app->db, args->table)) {
    return missing_table(app, args->table);
  }
  return render_table_as_grid(app, args->table);
}

