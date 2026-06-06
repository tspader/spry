#define SP_IMPLEMENTATION
#include "sp.h"

#define ABI_RUNTIME
#include "abi.h"

#include "json.h"

static sp_mem_t g_mem;
static bool g_mem_init = false;

static sp_mem_t rt_mem(void) {
  if (!g_mem_init) { g_mem = sp_mem_os_new(); g_mem_init = true; }
  return g_mem;
}

static void rt_fatal(sp_str_t msg) { host_fatal(msg.data, msg.len); }

static bool node_is(json_value_t* v, const c8* kind) {
  return v && v->type == JSON_STRING && sp_str_equal_cstr(v->string, kind);
}

static u32 dir_enum(json_value_t* v) {
  return node_is(v, "column") ? DIR_COLUMN : DIR_ROW;
}

static u32 align_enum(json_value_t* v) {
  if (node_is(v, "center")) return ALIGN_CENTER;
  if (node_is(v, "end")) return ALIGN_END;
  if (node_is(v, "stretch")) return ALIGN_STRETCH;
  return ALIGN_START;
}

static u32 justify_enum(json_value_t* v) {
  if (node_is(v, "center")) return JUSTIFY_CENTER;
  if (node_is(v, "end")) return JUSTIFY_END;
  if (node_is(v, "between")) return JUSTIFY_BETWEEN;
  return JUSTIFY_START;
}

static void set_str_prop(u32 handle, json_value_t* props, const c8* key, u32 sattr) {
  json_value_t* v = json_get(props, key);
  if (v && v->type == JSON_STRING) host_set_attr_str(handle, sattr, v->string.data, v->string.len);
}

static u32 render_node(json_value_t* node) {
  json_value_t* kind = json_get(node, "kind");

  u32 el;
  if (node_is(kind, "box")) el = EL_BOX;
  else if (node_is(kind, "text")) el = EL_TEXT;
  else if (node_is(kind, "link")) el = EL_LINK;
  else { rt_fatal(sp_str_lit("unknown node kind")); return HANDLE_NONE; }

  u32 handle = host_create_element(el);
  json_value_t* props = json_get(node, "props");

  switch (el) {
    case EL_BOX: {
      json_value_t* dir = json_get(props, "direction");
      if (dir) host_set_attr(handle, ATTR_DIRECTION, (s32)dir_enum(dir));
      json_value_t* gap = json_get(props, "gap");
      if (gap && gap->type == JSON_NUMBER) host_set_attr(handle, ATTR_GAP, (s32)gap->number);
      json_value_t* pad = json_get(props, "padding");
      if (pad && pad->type == JSON_NUMBER) host_set_attr(handle, ATTR_PADDING, (s32)pad->number);
      json_value_t* align = json_get(props, "align");
      if (align) host_set_attr(handle, ATTR_ALIGN, (s32)align_enum(align));
      json_value_t* justify = json_get(props, "justify");
      if (justify) host_set_attr(handle, ATTR_JUSTIFY, (s32)justify_enum(justify));

      json_value_t* children = json_get(node, "children");
      if (children && children->type == JSON_ARRAY) {
        for (json_value_t* c = children->first_child; c; c = c->next) {
          u32 child = render_node(c);
          if (child == HANDLE_NONE) return HANDLE_NONE;
          host_append_child(handle, child);
        }
      }
      break;
    }
    case EL_TEXT: {
      set_str_prop(handle, props, "text", SATTR_TEXT);
      break;
    }
    case EL_LINK: {
      set_str_prop(handle, props, "text", SATTR_TEXT);
      set_str_prop(handle, props, "href", SATTR_HREF);
      break;
    }
  }

  return handle;
}

__attribute__((export_name("alloc")))
void* rt_alloc(u32 len) {
  return sp_alloc(rt_mem(), len);
}

__attribute__((export_name("render")))
s32 rt_render(const c8* ptr, u32 len) {
  u32 caps = host_capabilities();
  if ((caps & ABI_REQUIRED_CAPS) != ABI_REQUIRED_CAPS) {
    rt_fatal(sp_str_lit("host is missing required capabilities"));
    return 1;
  }

  sp_str_t src = { .data = ptr, .len = len };
  json_value_t* root = json_parse(rt_mem(), src);
  if (!root) { rt_fatal(sp_str_lit("json parse error")); return 2; }

  u32 handle = render_node(root);
  if (handle == HANDLE_NONE) return 3;

  host_set_root(handle);
  return 0;
}
