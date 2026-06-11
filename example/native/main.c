#include "abi/endpoints.gen.h"
#include "sp.h"
#include "spry/ast.h"
#include "spry/core.h"
#include "spry/gtk.h"
#include "app/app.h"
#include "db/db.h"

typedef enum {
  APP_OK,
  APP_INVALID_COMMAND_LINE,
  APP_ERR_UI_NOT_FOUND,
  APP_ERR_ENDPOINTS_NOT_FOUND,
  APP_ERR_ENDPOINTS_INVALID,
  APP_ERR_DB_INVALID,
} app_err_t;

typedef struct {
  app_err_t err;
  sp_mem_t mem;
  spry_endpoints_t endpoints;
  sqlite3* db;
  struct {
    sp_str_t ui;
    sp_str_t endpoints;
  } args;
} app_t;

#define app_try(expr, as) do { s32 rc = (expr); if (rc) { app.err = as; goto done; }} while (0)

s32 main(s32 num_args, const c8** args) {
  app_t app = sp_zero;
  app_try(num_args < 3, APP_INVALID_COMMAND_LINE);
  app.args.ui = sp_cstr_as_str(args[1]);
  app.args.endpoints = sp_cstr_as_str(args[2]);
  app.mem = sp_mem_os_new();

  struct { sp_str_t ui; sp_str_t endpoints; } json = sp_zero;
  app_try(sp_io_read_file(app.mem, app.args.ui, &json.ui), APP_ERR_UI_NOT_FOUND);
  app_try(sp_io_read_file(app.mem, app.args.endpoints, &json.endpoints), APP_ERR_ENDPOINTS_NOT_FOUND);

  spry_issue_t issue = sp_zero_s(spry_issue_t);
  app_try(spry_endpoints_parse(app.mem, json.endpoints, &app.endpoints, &issue), APP_ERR_ENDPOINTS_INVALID);

  app_try(demo_db_open(&app.db), APP_ERR_DB_INVALID);

  demo_ctx_t* demo = demo_new(app.mem, app.db, app.endpoints);
  s32 status = spry_gtk_run(&(spry_gtk_opts_t){
    .title = "spry gtk demo",
    .tree = json.ui,
    .endpoints = json.endpoints,
    .rpc = demo->rpc,
  });

done:
  if (app.db) sqlite3_close(app.db);
  switch (app.err) {
    case APP_OK: break;
    case APP_INVALID_COMMAND_LINE: sp_log("usage: app [ui.json] [endpoints.json]"); break;
    case APP_ERR_UI_NOT_FOUND: sp_log("failed to load UI from {.cyan}", sp_fmt_str(app.args.ui)); break;
    case APP_ERR_ENDPOINTS_NOT_FOUND: sp_log("failed to load endpoints from {.cyan}", sp_fmt_str(app.args.endpoints)); break;
    case APP_ERR_ENDPOINTS_INVALID:  sp_log("failed to parse endpoints: {}", sp_fmt_str(spry_issue_str(app.mem, &issue)));
    case APP_ERR_DB_INVALID: {
      sp_log("failed to open in-memory db: {}", sp_fmt_str(demo_db_error(app.db)));
      break;
    }
  }

  return app.err;
}
