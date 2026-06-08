#define SP_IMPLEMENTATION
#include "sp.h"

#define ABI_RUNTIME
#include "spry/abi.h"

#include "yyjson.h"

static sp_mem_t g_mem;
static bool g_mem_init = false;

static sp_mem_t rt_mem(void) {
  if (!g_mem_init) { g_mem = sp_mem_os_new(); g_mem_init = true; }
  return g_mem;
}

static void rt_fatal(sp_str_t msg) { host_fatal(msg.data, msg.len); }

static u8 g_json_pool[1u << 20];

static yyjson_doc* rt_parse(const c8* ptr, u32 len) {
  yyjson_alc alc;
  if (!yyjson_alc_pool_init(&alc, g_json_pool, sizeof(g_json_pool))) return SP_NULLPTR;
  return yyjson_read_opts((c8*)ptr, len, 0, &alc, SP_NULLPTR);
}

static sp_str_t rt_str(yyjson_val* v) {
  if (!yyjson_is_str(v)) return sp_zero_s(sp_str_t);
  return (sp_str_t) { .data = yyjson_get_str(v), .len = (u32)yyjson_get_len(v) };
}

typedef struct rt_id {
  sp_str_t key;
  u32 handle;
  struct rt_id* next;
} rt_id_t;

typedef struct rt_field {
  u32 handle;
  sp_str_t name;
  struct rt_field* next;
} rt_field_t;

typedef struct rt_token {
  sp_str_t action;
  sp_str_t target;
  struct rt_token* next;
} rt_token_t;

static rt_id_t* g_ids;
static rt_field_t* g_fields;
static rt_field_t* g_fields_tail;
static rt_token_t* g_tokens;
static rt_token_t* g_tokens_tail;
static u32 g_token_count;

static void rt_reset(void) {
  g_ids = SP_NULLPTR;
  g_fields = SP_NULLPTR;
  g_fields_tail = SP_NULLPTR;
  g_tokens = SP_NULLPTR;
  g_tokens_tail = SP_NULLPTR;
  g_token_count = 0;
}

static void rt_register_id(sp_str_t key, u32 handle) {
  rt_id_t* node = sp_alloc_type(rt_mem(), rt_id_t);
  *node = sp_zero_s(rt_id_t);
  node->key = key;
  node->handle = handle;
  node->next = g_ids;
  g_ids = node;
}

static u32 rt_lookup_id(sp_str_t key) {
  for (rt_id_t* node = g_ids; node; node = node->next) {
    if (sp_str_equal(node->key, key)) return node->handle;
  }
  return HANDLE_NONE;
}

static void rt_push_field(u32 handle, sp_str_t name) {
  rt_field_t* node = sp_alloc_type(rt_mem(), rt_field_t);
  *node = sp_zero_s(rt_field_t);
  node->handle = handle;
  node->name = name;
  if (g_fields_tail) g_fields_tail->next = node; else g_fields = node;
  g_fields_tail = node;
}

static u32 rt_push_token(sp_str_t action, sp_str_t target) {
  rt_token_t* node = sp_alloc_type(rt_mem(), rt_token_t);
  *node = sp_zero_s(rt_token_t);
  node->action = action;
  node->target = target;
  if (g_tokens_tail) g_tokens_tail->next = node; else g_tokens = node;
  g_tokens_tail = node;
  return g_token_count++;
}

static rt_token_t* rt_token_at(u32 token) {
  rt_token_t* node = g_tokens;
  for (u32 i = 0; i < token && node; i++) node = node->next;
  return node;
}

static bool node_is(yyjson_val* v, const c8* kind) {
  return yyjson_equals_str(v, kind);
}

static u32 dir_enum(yyjson_val* v) {
  return node_is(v, "column") ? DIR_COLUMN : DIR_ROW;
}

static u32 align_enum(yyjson_val* v) {
  if (node_is(v, "center")) return ALIGN_CENTER;
  if (node_is(v, "end")) return ALIGN_END;
  if (node_is(v, "stretch")) return ALIGN_STRETCH;
  return ALIGN_START;
}

static u32 justify_enum(yyjson_val* v) {
  if (node_is(v, "center")) return JUSTIFY_CENTER;
  if (node_is(v, "end")) return JUSTIFY_END;
  if (node_is(v, "between")) return JUSTIFY_BETWEEN;
  return JUSTIFY_START;
}

static void set_str_prop(u32 handle, yyjson_val* props, const c8* key, u32 sattr) {
  yyjson_val* v = yyjson_obj_get(props, key);
  if (yyjson_is_str(v)) {
    sp_str_t s = rt_str(v);
    host_set_attr_str(handle, sattr, s.data, s.len);
  }
}

static u32 render_node(yyjson_val* node) {
  yyjson_val* kind = yyjson_obj_get(node, "kind");

  el_kind_t el;
  if (node_is(kind, "box")) el = EL_BOX;
  else if (node_is(kind, "text")) el = EL_TEXT;
  else if (node_is(kind, "link")) el = EL_LINK;
  else if (node_is(kind, "input")) el = EL_INPUT;
  else if (node_is(kind, "button")) el = EL_BUTTON;
  else { rt_fatal(sp_str_lit("unknown node kind")); return HANDLE_NONE; }

  u32 handle = host_create_element(el);
  yyjson_val* props = yyjson_obj_get(node, "props");

  yyjson_val* id = yyjson_obj_get(node, "id");
  if (yyjson_is_str(id)) rt_register_id(sp_str_copy(rt_mem(), rt_str(id)), handle);

  yyjson_val* on = yyjson_obj_get(node, "on");
  if (yyjson_is_obj(on)) {
    yyjson_val* action = yyjson_obj_get(on, "action");
    yyjson_val* target = yyjson_obj_get(on, "target");
    if (yyjson_is_str(action) && yyjson_is_str(target)) {
      yyjson_val* ev = yyjson_obj_get(on, "event");
      u32 event = node_is(ev, "submit") ? EVENT_SUBMIT : EVENT_CLICK;
      u32 token = rt_push_token(sp_str_copy(rt_mem(), rt_str(action)),
                                sp_str_copy(rt_mem(), rt_str(target)));
      host_on_event(handle, event, token);
    }
  }

  switch (el) {
    case EL_BOX: {
      yyjson_val* dir = yyjson_obj_get(props, "direction");
      if (dir) host_set_attr(handle, ATTR_DIRECTION, (s32)dir_enum(dir));
      yyjson_val* gap = yyjson_obj_get(props, "gap");
      if (yyjson_is_num(gap)) host_set_attr(handle, ATTR_GAP, (s32)yyjson_get_num(gap));
      yyjson_val* pad = yyjson_obj_get(props, "padding");
      if (yyjson_is_num(pad)) host_set_attr(handle, ATTR_PADDING, (s32)yyjson_get_num(pad));
      yyjson_val* align = yyjson_obj_get(props, "align");
      if (align) host_set_attr(handle, ATTR_ALIGN, (s32)align_enum(align));
      yyjson_val* justify = yyjson_obj_get(props, "justify");
      if (justify) host_set_attr(handle, ATTR_JUSTIFY, (s32)justify_enum(justify));

      yyjson_val* children = yyjson_obj_get(node, "children");
      if (yyjson_is_arr(children)) {
        u32 count = (u32)yyjson_arr_size(children);
        sp_for(i, count) {
          u32 child = render_node(yyjson_arr_get(children, i));
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
    case EL_INPUT: {
      yyjson_val* name = yyjson_obj_get(props, "name");
      if (yyjson_is_str(name)) {
        sp_str_t s = rt_str(name);
        host_set_attr_str(handle, SATTR_NAME, s.data, s.len);
        rt_push_field(handle, sp_str_copy(rt_mem(), s));
      }
      set_str_prop(handle, props, "value", SATTR_VALUE);
      set_str_prop(handle, props, "placeholder", SATTR_PLACEHOLDER);
      break;
    }
    case EL_BUTTON: {
      set_str_prop(handle, props, "text", SATTR_TEXT);
      break;
    }
  }

  return handle;
}

static void render_into(u32 target, yyjson_val* frag) {
  host_clear_children(target);
  if (yyjson_is_arr(frag)) {
    u32 count = (u32)yyjson_arr_size(frag);
    sp_for(i, count) {
      u32 child = render_node(yyjson_arr_get(frag, i));
      if (child != HANDLE_NONE) host_append_child(target, child);
    }
  } else {
    u32 child = render_node(frag);
    if (child != HANDLE_NONE) host_append_child(target, child);
  }
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

  rt_reset();

  yyjson_doc* doc = rt_parse(ptr, len);
  if (!doc) { rt_fatal(sp_str_lit("json parse error")); return 2; }

  u32 handle = render_node(yyjson_doc_get_root(doc));
  if (handle == HANDLE_NONE) return 3;

  host_set_root(handle);
  return 0;
}

__attribute__((export_name("dispatch")))
void rt_dispatch(u32 token) {
  rt_token_t* it = rt_token_at(token);
  if (!it) return;

  sp_da(c8) body = sp_da_new(rt_mem(), c8);
  bool first = true;
  for (rt_field_t* f = g_fields; f; f = f->next) {
    c8 val[1024];
    u32 vlen = host_get_value(f->handle, val, sizeof(val));
    if (!first) sp_da_push(body, '&');
    first = false;
    sp_for(i, f->name.len) sp_da_push(body, f->name.data[i]);
    sp_da_push(body, '=');
    sp_for(i, vlen) sp_da_push(body, val[i]);
  }

  host_submit(token, it->action.data, it->action.len, body, (u32)sp_da_size(body));
}

__attribute__((export_name("deliver")))
void rt_deliver(u32 token, const c8* ptr, u32 len) {
  rt_token_t* it = rt_token_at(token);
  if (!it) return;

  u32 target = rt_lookup_id(it->target);
  if (target == HANDLE_NONE) { rt_fatal(sp_str_lit("swap target not found")); return; }

  yyjson_doc* doc = rt_parse(ptr, len);
  if (!doc) { rt_fatal(sp_str_lit("fragment parse error")); return; }

  render_into(target, yyjson_doc_get_root(doc));
}
