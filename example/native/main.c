#include <gtk/gtk.h>
#include <sqlite3.h>
#include "sp.h"
#include "spry/host.h"
#include "spry/backend/gtk.h"
#include "spry/rpc.h"
#include "handlers.h"

typedef struct {
  sp_mem_t mem;
  sp_str_t wasm_path;
  sp_str_t tree;
  sp_str_t endpoints;
  demo_ctx_t* demo;
  wasm_host_t* host;
  backend_t backend;
} app_ctx_t;

static void on_activate(GtkApplication* app, gpointer user_data) {
  app_ctx_t* ctx = user_data;

  GtkWidget* window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "spry sqlite browser — gtk");
  gtk_window_set_default_size(GTK_WINDOW(window), 900, 700);

  ctx->host = wasm_host_new(ctx->mem, ctx->wasm_path);
  if (!ctx->host) {
    sp_log("native: failed to create wasm host");
    return;
  }

  ctx->backend = gtk_backend_make(ctx->mem, GTK_WINDOW(window), wasm_host_iface(ctx->host), spry_rpc_resolver, ctx->demo->rpc);
  wasm_host_set_backend(ctx->host, &ctx->backend);

  s32 erc = wasm_host_endpoints(ctx->host, ctx->endpoints);
  if (erc != 0) {
    sp_log("native: endpoints returned {}", sp_fmt_int(erc));
    return;
  }

  s32 rc = wasm_host_render(ctx->host, ctx->tree);
  if (rc != 0) sp_log("native: render returned {}", sp_fmt_int(rc));

  gtk_window_present(GTK_WINDOW(window));
}

s32 main(s32 argc, c8** argv) {
  if (argc < 4) {
    sp_log("usage: app <ui.json> <endpoints.json> <db.sqlite> [runtime.wasm]");
    return 1;
  }

  sp_mem_t mem = sp_mem_os_new();
  app_ctx_t ctx = sp_zero_s(app_ctx_t);
  ctx.mem = mem;
  ctx.wasm_path = argc >= 5 ? sp_cstr_as_str(argv[4]) : sp_str_lit("runtime.wasm");
  if (sp_io_read_file(mem, sp_cstr_as_str(argv[1]), &ctx.tree)) {
    sp_log("native: cannot read tree {}", sp_fmt_cstr(argv[1]));
    return 1;
  }
  if (sp_io_read_file(mem, sp_cstr_as_str(argv[2]), &ctx.endpoints)) {
    sp_log("native: cannot read endpoints {}", sp_fmt_cstr(argv[2]));
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
  ctx.demo = demo_new(mem, db);

  GtkApplication* app = gtk_application_new("dev.demo.hypermedia", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), &ctx);
  s32 status = g_application_run(G_APPLICATION(app), 1, argv);
  g_object_unref(app);
  sqlite3_close(db);
  return status;
}
