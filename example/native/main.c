#include <gtk/gtk.h>
#include "sp.h"
#include "spry/host.h"
#include "spry/backend/gtk.h"

typedef struct {
  sp_mem_t mem;
  sp_str_t wasm_path;
  sp_str_t tree;
  wasm_host_t* host;
  backend_t backend;
} app_ctx_t;

static sp_str_t app_request(void* ctx, sp_str_t action, sp_str_t body) {
  app_ctx_t* app = ctx;
  (void)action;

  sp_str_t name = sp_str_lit("stranger");
  sp_for(i, body.len) {
    if (body.data[i] == '=') {
      sp_str_t rest = { .data = body.data + i + 1, .len = body.len - i - 1 };
      if (!sp_str_empty(rest)) name = rest;
      break;
    }
  }

  sp_str_t out = sp_str_concat(app->mem, sp_str_lit("{\"kind\":\"text\",\"props\":{\"text\":\"Hello, "), name);
  return sp_str_concat(app->mem, out, sp_str_lit("!\"}}"));
}

static void on_activate(GtkApplication* app, gpointer user_data) {
  app_ctx_t* ctx = user_data;

  GtkWidget* window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "hypermedia runtime — gtk");
  gtk_window_set_default_size(GTK_WINDOW(window), 480, 360);

  ctx->host = wasm_host_new(ctx->mem, ctx->wasm_path);
  if (!ctx->host) {
    sp_log("native: failed to create wasm host");
    return;
  }

  ctx->backend = gtk_backend_make(ctx->mem, GTK_WINDOW(window), wasm_host_iface(ctx->host), app_request, ctx);
  wasm_host_set_backend(ctx->host, &ctx->backend);

  s32 rc = wasm_host_render(ctx->host, ctx->tree);
  if (rc != 0) sp_log("native: render returned {}", sp_fmt_int(rc));

  gtk_window_present(GTK_WINDOW(window));
}

s32 main(s32 argc, c8** argv) {
  if (argc < 2) {
    sp_log("usage: app <ui.json> [runtime.wasm]");
    return 1;
  }

  sp_mem_t mem = sp_mem_os_new();
  app_ctx_t ctx = sp_zero_s(app_ctx_t);
  ctx.mem = mem;
  ctx.wasm_path = argc >= 3 ? sp_cstr_as_str(argv[2]) : sp_str_lit("runtime.wasm");
  if (sp_io_read_file(mem, sp_cstr_as_str(argv[1]), &ctx.tree) != SP_OK) {
    sp_log("native: cannot read tree {}", sp_fmt_cstr(argv[1]));
    return 1;
  }

  GtkApplication* app = gtk_application_new("dev.demo.hypermedia", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), &ctx);
  s32 status = g_application_run(G_APPLICATION(app), 1, argv);
  g_object_unref(app);
  return status;
}
