#include <gtk/gtk.h>
#include "sp.h"
#include "spry/gtk.h"
#include "spry/host.h"
#include "spry/backend/gtk.h"

typedef struct {
  sp_mem_t mem;
  const spry_gtk_opts_t* opts;
  wasm_host_t* host;
  backend_t backend;
  s32 rc;
} spry_gtk_ctx_t;

static void spry_gtk_activate(GtkApplication* app, gpointer user_data) {
  spry_gtk_ctx_t* ctx = user_data;
  const spry_gtk_opts_t* opts = ctx->opts;

  GtkWidget* window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), sp_str_to_cstr(ctx->mem, opts->title));
  gtk_window_set_default_size(GTK_WINDOW(window), opts->width ? (s32)opts->width : 900, opts->height ? (s32)opts->height : 700);

  ctx->host = wasm_host_new(ctx->mem, opts->wasm_path);
  if (!ctx->host) {
    sp_log("spry: failed to create wasm host");
    ctx->rc = 1;
    return;
  }

  ctx->backend = gtk_backend_make(ctx->mem, GTK_WINDOW(window), wasm_host_iface(ctx->host), spry_rpc_resolver, opts->rpc);
  wasm_host_set_backend(ctx->host, &ctx->backend);

  s32 erc = wasm_host_endpoints(ctx->host, opts->endpoints);
  if (erc != 0) {
    sp_log("spry: endpoints returned {}", sp_fmt_int(erc));
    ctx->rc = 1;
    return;
  }

  s32 rc = wasm_host_render(ctx->host, opts->tree);
  if (rc != 0) {
    sp_log("spry: render returned {}", sp_fmt_int(rc));
    ctx->rc = 1;
    return;
  }

  gtk_window_present(GTK_WINDOW(window));
}

s32 spry_gtk_run(const spry_gtk_opts_t* opts) {
  if (opts->rpc && !spry_rpc_check(opts->rpc)) return 1;

  sp_mem_t mem = sp_mem_os_new();
  spry_gtk_ctx_t* ctx = sp_alloc_type(mem, spry_gtk_ctx_t);
  *ctx = sp_zero_s(spry_gtk_ctx_t);
  ctx->mem = mem;
  ctx->opts = opts;

  GtkApplication* app = gtk_application_new("dev.spry.app", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(spry_gtk_activate), ctx);
  s32 status = g_application_run(G_APPLICATION(app), 0, SP_NULLPTR);
  g_object_unref(app);
  return ctx->rc ? ctx->rc : status;
}
