#include <gtk/gtk.h>
#include "sp.h"
#include "wasm_host.h"
#include "gtk_backend.h"

typedef struct {
  sp_mem_t mem;
  sp_str_t wasm_path;
  sp_str_t tree;
} app_ctx_t;

static void on_activate(GtkApplication* app, gpointer user_data) {
  app_ctx_t* ctx = user_data;

  GtkWidget* window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "hypermedia runtime — gtk");
  gtk_window_set_default_size(GTK_WINDOW(window), 480, 360);

  backend_t backend = gtk_backend_make(ctx->mem, GTK_WINDOW(window));
  wasm_host_t* host = wasm_host_new(ctx->mem, ctx->wasm_path, &backend);
  if (!host) {
    sp_log("native: failed to create wasm host");
    return;
  }

  s32 rc = wasm_host_render(host, ctx->tree);
  if (rc != 0) sp_log("native: render returned {}", sp_fmt_int(rc));

  gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char** argv) {
  if (argc < 3) {
    sp_log("usage: native <runtime.wasm> <tree.json>");
    return 1;
  }

  sp_mem_t mem = sp_mem_os_new();
  app_ctx_t ctx = sp_zero_s(app_ctx_t);
  ctx.mem = mem;
  ctx.wasm_path = sp_cstr_as_str(argv[1]);
  if (sp_io_read_file(mem, sp_cstr_as_str(argv[2]), &ctx.tree) != SP_OK) {
    sp_log("native: cannot read tree {}", sp_fmt_cstr(argv[2]));
    return 1;
  }

  GtkApplication* app = gtk_application_new("dev.demo.hypermedia", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), &ctx);
  int status = g_application_run(G_APPLICATION(app), 1, argv);
  g_object_unref(app);
  return status;
}
