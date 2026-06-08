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
  rt_id_t* node = (rt_id_t*)sp_alloc(rt_mem(), sizeof(rt_id_t));
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
  rt_field_t* node = (rt_field_t*)sp_alloc(rt_mem(), sizeof(rt_field_t));
  *node = sp_zero_s(rt_field_t);
  node->handle = handle;
  node->name = name;
  if (g_fields_tail) g_fields_tail->next = node; else g_fields = node;
  g_fields_tail = node;
}

static u32 rt_push_token(sp_str_t action, sp_str_t target) {
  rt_token_t* node = (rt_token_t*)sp_alloc(rt_mem(), sizeof(rt_token_t));
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

static u32 rt_buf_push(c8* buf, u32 cap, u32 len, c8 c) {
  if (len < cap) buf[len++] = c;
  return len;
}

static u32 rt_buf_append(c8* buf, u32 cap, u32 len, sp_str_t s) {
  sp_for(i, s.len) { if (len >= cap) break; buf[len++] = s.data[i]; }
  return len;
}

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

  el_kind_t el;
  if (node_is(kind, "box")) el = EL_BOX;
  else if (node_is(kind, "text")) el = EL_TEXT;
  else if (node_is(kind, "link")) el = EL_LINK;
  else if (node_is(kind, "input")) el = EL_INPUT;
  else if (node_is(kind, "button")) el = EL_BUTTON;
  else { rt_fatal(sp_str_lit("unknown node kind")); return HANDLE_NONE; }

  u32 handle = host_create_element(el);
  json_value_t* props = json_get(node, "props");

  json_value_t* id = json_get(node, "id");
  if (id && id->type == JSON_STRING) rt_register_id(id->string, handle);

  json_value_t* on = json_get(node, "on");
  if (on && on->type == JSON_OBJECT) {
    json_value_t* action = json_get(on, "action");
    json_value_t* target = json_get(on, "target");
    if (action && action->type == JSON_STRING && target && target->type == JSON_STRING) {
      json_value_t* ev = json_get(on, "event");
      u32 event = node_is(ev, "submit") ? EVENT_SUBMIT : EVENT_CLICK;
      u32 token = rt_push_token(action->string, target->string);
      host_on_event(handle, event, token);
    }
  }

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
    case EL_INPUT: {
      json_value_t* name = json_get(props, "name");
      if (name && name->type == JSON_STRING) {
        host_set_attr_str(handle, SATTR_NAME, name->string.data, name->string.len);
        rt_push_field(handle, name->string);
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

static void render_into(u32 target, json_value_t* frag) {
  host_clear_children(target);
  if (frag->type == JSON_ARRAY) {
    for (json_value_t* c = frag->first_child; c; c = c->next) {
      u32 child = render_node(c);
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

  sp_str_t src = { .data = ptr, .len = len };
  json_value_t* root = json_parse(rt_mem(), src);
  if (!root) { rt_fatal(sp_str_lit("json parse error")); return 2; }

  u32 handle = render_node(root);
  if (handle == HANDLE_NONE) return 3;

  host_set_root(handle);
  return 0;
}

__attribute__((export_name("dispatch")))
void rt_dispatch(u32 token) {
  rt_token_t* it = rt_token_at(token);
  if (!it) return;

  c8 body[4096];
  u32 blen = 0;
  bool first = true;
  for (rt_field_t* f = g_fields; f; f = f->next) {
    c8 val[1024];
    u32 vlen = host_get_value(f->handle, val, sizeof(val));
    sp_str_t value = { .data = val, .len = vlen };
    if (!first) blen = rt_buf_push(body, sizeof(body), blen, '&');
    first = false;
    blen = rt_buf_append(body, sizeof(body), blen, f->name);
    blen = rt_buf_push(body, sizeof(body), blen, '=');
    blen = rt_buf_append(body, sizeof(body), blen, value);
  }

  host_submit(token, it->action.data, it->action.len, body, blen);
}

__attribute__((export_name("deliver")))
void rt_deliver(u32 token, const c8* ptr, u32 len) {
  rt_token_t* it = rt_token_at(token);
  if (!it) return;

  u32 target = rt_lookup_id(it->target);
  if (target == HANDLE_NONE) { rt_fatal(sp_str_lit("swap target not found")); return; }

  sp_str_t src = { .data = ptr, .len = len };
  json_value_t* frag = json_parse(rt_mem(), src);
  if (!frag) { rt_fatal(sp_str_lit("fragment parse error")); return; }

  render_into(target, frag);
}
