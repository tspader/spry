#include <gtk/gtk.h>
#include "sp.h"
#include "abi.h"
#include "backend.h"
#include "gtk_backend.h"

typedef struct {
  GtkWindow* window;
  sp_mem_t mem;
  host_iface_t host;
  gtk_request_fn_t resolver;
  void* resolver_ctx;
} gtk_ctx_t;

typedef struct {
  host_iface_t host;
  u32 token;
  sp_str_t json;
} gtk_deliver_t;

static GtkAlign align_to_gtk(s32 value) {
  switch (value) {
    case ALIGN_START:   return GTK_ALIGN_START;
    case ALIGN_CENTER:  return GTK_ALIGN_CENTER;
    case ALIGN_END:     return GTK_ALIGN_END;
    case ALIGN_STRETCH: return GTK_ALIGN_FILL;
  }
  return GTK_ALIGN_START;
}

static u32 gtk_caps(void* self) {
  (void)self;
  return ABI_REQUIRED_CAPS;
}

static void* gtk_create(void* self, u32 kind) {
  (void)self;
  switch (kind) {
    case EL_BOX:    return gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    case EL_TEXT:   return gtk_label_new(SP_NULLPTR);
    case EL_LINK:   return gtk_link_button_new("");
    case EL_INPUT:  return gtk_entry_new();
    case EL_BUTTON: return gtk_button_new();
  }
  return SP_NULLPTR;
}

static void gtk_set_attr(void* self, void* node, u32 attr, s32 value) {
  (void)self;
  GtkWidget* w = node;
  switch (attr) {
    case ATTR_DIRECTION:
      gtk_orientable_set_orientation(GTK_ORIENTABLE(w),
        value == DIR_COLUMN ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
      break;
    case ATTR_GAP:
      gtk_box_set_spacing(GTK_BOX(w), value);
      break;
    case ATTR_PADDING:
      gtk_widget_set_margin_start(w, value);
      gtk_widget_set_margin_end(w, value);
      gtk_widget_set_margin_top(w, value);
      gtk_widget_set_margin_bottom(w, value);
      break;
    case ATTR_ALIGN:
      g_object_set_data(G_OBJECT(w), "demo-align", GINT_TO_POINTER(value + 1));
      break;
    case ATTR_JUSTIFY:
      g_object_set_data(G_OBJECT(w), "demo-justify", GINT_TO_POINTER(value + 1));
      break;
  }
}

static void gtk_set_attr_str(void* self, void* node, u32 attr, sp_str_t value) {
  gtk_ctx_t* ctx = self;
  GtkWidget* w = node;
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(ctx->mem);
  c8* cstr = sp_str_to_cstr(scratch.mem, value);
  switch (attr) {
    case SATTR_TEXT:
      if (GTK_IS_LABEL(w)) gtk_label_set_text(GTK_LABEL(w), cstr);
      else if (GTK_IS_BUTTON(w)) gtk_button_set_label(GTK_BUTTON(w), cstr);
      break;
    case SATTR_HREF:
      if (GTK_IS_LINK_BUTTON(w)) gtk_link_button_set_uri(GTK_LINK_BUTTON(w), cstr);
      break;
    case SATTR_VALUE:
      if (GTK_IS_EDITABLE(w)) gtk_editable_set_text(GTK_EDITABLE(w), cstr);
      break;
    case SATTR_NAME:
      break;
    case SATTR_PLACEHOLDER:
      if (GTK_IS_ENTRY(w)) gtk_entry_set_placeholder_text(GTK_ENTRY(w), cstr);
      break;
  }
  sp_mem_end_scratch(scratch);
}

static void apply_child_layout(GtkWidget* parent, GtkWidget* child) {
  GtkOrientation o = gtk_orientable_get_orientation(GTK_ORIENTABLE(parent));
  s32 align_raw = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(parent), "demo-align"));
  s32 justify_raw = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(parent), "demo-justify"));

  if (align_raw) {
    GtkAlign a = align_to_gtk(align_raw - 1);
    if (o == GTK_ORIENTATION_VERTICAL) gtk_widget_set_halign(child, a);
    else gtk_widget_set_valign(child, a);
  }

  if (justify_raw) {
    s32 j = justify_raw - 1;
    gboolean expand = (j == JUSTIFY_CENTER || j == JUSTIFY_END || j == JUSTIFY_BETWEEN);
    GtkAlign a = (j == JUSTIFY_END) ? GTK_ALIGN_END : GTK_ALIGN_CENTER;
    if (o == GTK_ORIENTATION_HORIZONTAL) {
      gtk_widget_set_hexpand(child, expand);
      gtk_widget_set_halign(child, a);
    } else {
      gtk_widget_set_vexpand(child, expand);
      gtk_widget_set_valign(child, a);
    }
  }
}

static void gtk_append(void* self, void* parent, void* child) {
  (void)self;
  gtk_box_append(GTK_BOX(parent), child);
  apply_child_layout(parent, child);
}

static void gtk_set_root(void* self, void* node) {
  gtk_ctx_t* ctx = self;
  gtk_window_set_child(ctx->window, GTK_WIDGET(node));
}

static void on_widget_event(GtkWidget* w, gpointer user_data) {
  gtk_ctx_t* ctx = user_data;
  u32 token = (u32)GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(w), "demo-token"));
  ctx->host.dispatch(ctx->host.ctx, token);
}

static void gtk_on_event(void* self, void* node, u32 event, u32 token) {
  gtk_ctx_t* ctx = self;
  GtkWidget* w = node;
  g_object_set_data(G_OBJECT(w), "demo-token", GUINT_TO_POINTER(token));
  switch (event) {
    case EVENT_CLICK:  g_signal_connect(w, "clicked", G_CALLBACK(on_widget_event), ctx); break;
    case EVENT_SUBMIT: g_signal_connect(w, "activate", G_CALLBACK(on_widget_event), ctx); break;
  }
}

static gboolean gtk_deliver_idle(gpointer data) {
  gtk_deliver_t* d = data;
  d->host.deliver(d->host.ctx, d->token, d->json);
  return G_SOURCE_REMOVE;
}

static void gtk_submit(void* self, u32 token, sp_str_t action, sp_str_t body) {
  gtk_ctx_t* ctx = self;
  sp_str_t json = ctx->resolver(ctx->resolver_ctx, action, body);
  gtk_deliver_t* d = sp_alloc(ctx->mem, sizeof(gtk_deliver_t));
  *d = sp_zero_s(gtk_deliver_t);
  d->host = ctx->host;
  d->token = token;
  d->json = json;
  g_idle_add(gtk_deliver_idle, d);
}

static void gtk_clear_children(void* self, void* node) {
  (void)self;
  GtkWidget* parent = node;
  GtkWidget* child = gtk_widget_get_first_child(parent);
  while (child) {
    GtkWidget* next = gtk_widget_get_next_sibling(child);
    gtk_box_remove(GTK_BOX(parent), child);
    child = next;
  }
}

static u32 gtk_get_value(void* self, void* node, c8* out, u32 cap) {
  (void)self;
  GtkWidget* w = node;
  const c8* text = GTK_IS_EDITABLE(w) ? gtk_editable_get_text(GTK_EDITABLE(w)) : "";
  u32 n = 0;
  while (text[n] && n < cap) { out[n] = text[n]; n++; }
  return n;
}

static void gtk_fatal(void* self, sp_str_t message) {
  (void)self;
  sp_log("gtk backend fatal: {}", sp_fmt_str(message));
}

backend_t gtk_backend_make(sp_mem_t mem, GtkWindow* window, host_iface_t host, gtk_request_fn_t resolver, void* resolver_ctx) {
  gtk_ctx_t* ctx = sp_alloc(mem, sizeof(gtk_ctx_t));
  *ctx = sp_zero_s(gtk_ctx_t);
  ctx->window = window;
  ctx->mem = mem;
  ctx->host = host;
  ctx->resolver = resolver;
  ctx->resolver_ctx = resolver_ctx;
  return (backend_t){
    .self = ctx,
    .capabilities = gtk_caps,
    .create_element = gtk_create,
    .set_attr = gtk_set_attr,
    .set_attr_str = gtk_set_attr_str,
    .append_child = gtk_append,
    .set_root = gtk_set_root,
    .on_event = gtk_on_event,
    .submit = gtk_submit,
    .clear_children = gtk_clear_children,
    .get_value = gtk_get_value,
    .fatal = gtk_fatal,
  };
}
