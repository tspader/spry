#include <sqlite3.h>
#include "sp.h"
#include "spry/core.h"
#include "spry/gtk.h"
#include "handlers.h"

s32 main(s32 argc, c8** argv) {
  if (argc < 4) {
    sp_log("usage: app <ui.json> <endpoints.json> <db.sqlite>");
    return 1;
  }

  sp_mem_t mem = sp_mem_os_new();

  sp_str_t tree;
  if (sp_io_read_file(mem, sp_cstr_as_str(argv[1]), &tree) != SP_OK) {
    sp_log("native: cannot read tree {}", sp_fmt_cstr(argv[1]));
    return 1;
  }

  sp_str_t endpoints_json;
  if (sp_io_read_file(mem, sp_cstr_as_str(argv[2]), &endpoints_json) != SP_OK) {
    sp_log("native: cannot read endpoints {}", sp_fmt_cstr(argv[2]));
    return 1;
  }

  spry_endpoints_t endpoints;
  sp_str_t error = sp_zero_s(sp_str_t);
  if (!spry_endpoints_parse(mem, endpoints_json, &endpoints, &error)) {
    sp_log("native: invalid endpoints: {}", sp_fmt_str(error));
    return 1;
  }

  sqlite3* db;
  if (sqlite3_open(argv[3], &db) != SQLITE_OK) {
    sp_log("native: cannot open db {}", sp_fmt_cstr(argv[3]));
    return 1;
  }
  if (!demo_db_seed(db)) {
    sp_log("native: seed failed: {}", sp_fmt_cstr(sqlite3_errmsg(db)));
    return 1;
  }
  demo_ctx_t* demo = demo_new(mem, db, endpoints);

  s32 status = spry_gtk_run(&(spry_gtk_opts_t){
    .title = sp_str_lit("spry sqlite browser — gtk"),
    .tree = tree,
    .endpoints = endpoints_json,
    .rpc = demo->rpc,
  });
  sqlite3_close(db);
  return status;
}
