#define SP_IMPLEMENTATION
#include "sp.h"

#define ABI_RUNTIME
#include "spry/abi.h"

#include "yyjson.h"
#include "abi/ui.gen.h"

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

static sp_str_t node_id(const spry_node_t* node) {
  switch (node->kind) {
    case SPRY_NODE_KIND_BOX:    return node->as.box.id;
    case SPRY_NODE_KIND_TEXT:   return node->as.text.id;
    case SPRY_NODE_KIND_LINK:   return node->as.link.id;
    case SPRY_NODE_KIND_INPUT:  return node->as.input.id;
    case SPRY_NODE_KIND_BUTTON: return node->as.button.id;
  }
  return sp_zero_s(sp_str_t);
}

static const spry_interaction_t* node_on(const spry_node_t* node) {
  switch (node->kind) {
    case SPRY_NODE_KIND_BOX:    return node->as.box.on;
    case SPRY_NODE_KIND_BUTTON: return node->as.button.on;
    case SPRY_NODE_KIND_TEXT:   return SP_NULLPTR;
    case SPRY_NODE_KIND_LINK:   return SP_NULLPTR;
    case SPRY_NODE_KIND_INPUT:  return SP_NULLPTR;
  }
  return SP_NULLPTR;
}

static u32 render_node(const spry_node_t* node) {
  u32 handle = host_create_element((u32)node->kind);

  sp_str_t id = node_id(node);
  if (!sp_str_empty(id)) rt_register_id(id, handle);

  const spry_interaction_t* on = node_on(node);
  if (on) {
    u32 token = rt_push_token(on->action, on->target);
    host_on_event(handle, (u32)on->event, token);
  }

  switch (node->kind) {
    case SPRY_NODE_KIND_BOX: {
      const spry_box_t* box = &node->as.box;
      if (box->props) {
        const spry_box_props_t* props = box->props;
        host_set_direction(handle, (u32)props->direction);
        host_set_gap(handle, props->gap);
        host_set_padding(handle, props->padding);
        host_set_align(handle, (u32)props->align);
        host_set_justify(handle, (u32)props->justify);
      }
      sp_da_for(box->children, i) {
        host_append_child(handle, render_node(&box->children[i]));
      }
      break;
    }
    case SPRY_NODE_KIND_TEXT: {
      sp_str_t text = node->as.text.props.text;
      host_set_text(handle, text.data, text.len);
      break;
    }
    case SPRY_NODE_KIND_LINK: {
      const spry_link_props_t* props = &node->as.link.props;
      host_set_text(handle, props->text.data, props->text.len);
      host_set_href(handle, props->href.data, props->href.len);
      break;
    }
    case SPRY_NODE_KIND_INPUT: {
      const spry_input_props_t* props = &node->as.input.props;
      host_set_name(handle, props->name.data, props->name.len);
      rt_push_field(handle, props->name);
      if (!sp_str_empty(props->value)) host_set_value(handle, props->value.data, props->value.len);
      if (!sp_str_empty(props->placeholder)) host_set_placeholder(handle, props->placeholder.data, props->placeholder.len);
      break;
    }
    case SPRY_NODE_KIND_BUTTON: {
      sp_str_t text = node->as.button.props.text;
      host_set_text(handle, text.data, text.len);
      break;
    }
  }

  return handle;
}

static bool rt_parse_node(yyjson_val* val, spry_node_t* out) {
  spry_ctx_t ctx;
  spry_ctx_init(&ctx, rt_mem());
  if (spry_ast_parse(&SPRY_AST_ROOT_TYPE, val, &ctx, out) != SPRY_OK) {
    rt_fatal(spry_issue_str(rt_mem(), &ctx.issues[0]));
    return false;
  }
  return true;
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

  spry_node_t node = sp_zero_s(spry_node_t);
  if (!rt_parse_node(yyjson_doc_get_root(doc), &node)) return 3;

  host_set_root(render_node(&node));
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

  spry_node_t node = sp_zero_s(spry_node_t);
  if (!rt_parse_node(yyjson_doc_get_root(doc), &node)) return;

  host_clear_children(target);
  host_append_child(target, render_node(&node));
}
