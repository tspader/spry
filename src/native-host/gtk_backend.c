#include <gtk/gtk.h>
#include "sp.h"
#include "abi.h"
#include "backend.h"
#include "gtk_backend.h"

typedef struct {
  GtkWindow* window;
} gtk_ctx_t;

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
    case EL_BOX:  return gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    case EL_TEXT: return gtk_label_new(SP_NULLPTR);
    case EL_LINK: return gtk_link_button_new("");
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
  (void)self;
  GtkWidget* w = node;
  c8 buf[4096];
  u32 n = value.len < sizeof(buf) - 1 ? value.len : (u32)sizeof(buf) - 1;
  memcpy(buf, value.data, n);
  buf[n] = 0;
  switch (attr) {
    case SATTR_TEXT:
      if (GTK_IS_LABEL(w)) gtk_label_set_text(GTK_LABEL(w), buf);
      else if (GTK_IS_BUTTON(w)) gtk_button_set_label(GTK_BUTTON(w), buf);
      break;
    case SATTR_HREF:
      if (GTK_IS_LINK_BUTTON(w)) gtk_link_button_set_uri(GTK_LINK_BUTTON(w), buf);
      break;
  }
}

static void apply_child_layout(GtkWidget* parent, GtkWidget* child) {
  GtkOrientation o = gtk_orientable_get_orientation(GTK_ORIENTABLE(parent));
  int align_raw = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(parent), "demo-align"));
  int justify_raw = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(parent), "demo-justify"));

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

static void gtk_fatal(void* self, sp_str_t message) {
  (void)self;
  sp_log("gtk backend fatal: {}", sp_fmt_str(message));
}

backend_t gtk_backend_make(sp_mem_t mem, GtkWindow* window) {
  gtk_ctx_t* ctx = sp_alloc(mem, sizeof(gtk_ctx_t));
  *ctx = sp_zero_s(gtk_ctx_t);
  ctx->window = window;
  return (backend_t){
    .self = ctx,
    .capabilities = gtk_caps,
    .create_element = gtk_create,
    .set_attr = gtk_set_attr,
    .set_attr_str = gtk_set_attr_str,
    .append_child = gtk_append,
    .set_root = gtk_set_root,
    .fatal = gtk_fatal,
  };
}
