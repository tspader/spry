#define SP_IMPLEMENTATION
#include "sp.h"

#define ABI_RUNTIME
#include "spry/abi.h"

#include "yyjson.h"
#include "abi/ui.gen.h"
#include "abi/fault.gen.h"
#include "abi/endpoints.gen.h"

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
  const spry_invoke_t* invoke;
  bool pending;
  struct rt_token* next;
} rt_token_t;

static rt_id_t* g_ids;
static rt_field_t* g_fields;
static rt_token_t* g_tokens;
static rt_token_t* g_tokens_tail;
static u32 g_token_count;
static spry_endpoints_t g_endpoints;

static void rt_reset(void) {
  g_ids = SP_NULLPTR;
  g_fields = SP_NULLPTR;
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
  node->next = g_fields;
  g_fields = node;
}

static u32 rt_find_field(sp_str_t name) {
  for (rt_field_t* node = g_fields; node; node = node->next) {
    if (sp_str_equal(node->name, name)) return node->handle;
  }
  return HANDLE_NONE;
}

static u32 rt_push_token(const spry_invoke_t* invoke) {
  rt_token_t* node = sp_alloc_type(rt_mem(), rt_token_t);
  *node = sp_zero_s(rt_token_t);
  node->invoke = invoke;
  if (g_tokens_tail) g_tokens_tail->next = node; else g_tokens = node;
  g_tokens_tail = node;
  return g_token_count++;
}

static rt_token_t* rt_token_at(u32 token) {
  rt_token_t* node = g_tokens;
  for (u32 i = 0; i < token && node; i++) node = node->next;
  return node;
}

static const spry_endpoint_t* rt_endpoint_find(sp_str_t name) {
  sp_da_for(g_endpoints, i) {
    if (sp_str_equal(g_endpoints[i].key, name)) return &g_endpoints[i].value;
  }
  return SP_NULLPTR;
}

typedef struct {
  const spry_endpoint_arg_t* arg;
  bool required;
} rt_arg_ref_t;

static rt_arg_ref_t rt_endpoint_arg_find(const spry_endpoint_t* ep, sp_str_t name) {
  sp_da_for(ep->args.properties, i) {
    if (sp_str_equal(ep->args.properties[i].key, name)) {
      return (rt_arg_ref_t){ .arg = &ep->args.properties[i].value, .required = true };
    }
  }
  sp_da_for(ep->args.optionalProperties, i) {
    if (sp_str_equal(ep->args.optionalProperties[i].key, name)) {
      return (rt_arg_ref_t){ .arg = &ep->args.optionalProperties[i].value, .required = false };
    }
  }
  return sp_zero_s(rt_arg_ref_t);
}

static sp_str_t rt_invoke_body_find(const spry_invoke_t* invoke, sp_str_t name, bool* found) {
  sp_da_for(invoke->body, i) {
    if (sp_str_equal(invoke->body[i].key, name)) {
      *found = true;
      return invoke->body[i].value;
    }
  }
  *found = false;
  return sp_zero_s(sp_str_t);
}

static void rt_out_raw(sp_da(c8)* out, sp_str_t value) {
  sp_for(i, value.len) sp_da_push(*out, value.data[i]);
}

static void rt_out_json_str(sp_da(c8)* out, sp_str_t value) {
  static const c8 hex[] = "0123456789abcdef";
  sp_da_push(*out, '"');
  sp_for(i, value.len) {
    c8 c = value.data[i];
    if (c == '"' || c == '\\') {
      sp_da_push(*out, '\\');
      sp_da_push(*out, c);
    } else if ((u8)c < 0x20) {
      rt_out_raw(out, sp_str_lit("\\u00"));
      sp_da_push(*out, hex[((u8)c >> 4) & 0xf]);
      sp_da_push(*out, hex[(u8)c & 0xf]);
    } else {
      sp_da_push(*out, c);
    }
  }
  sp_da_push(*out, '"');
}

static bool rt_number_valid(sp_str_t value) {
  u32 i = 0;
  if (i < value.len && value.data[i] == '-') i++;
  if (i >= value.len) return false;
  if (value.data[i] == '0') {
    i++;
  } else if (value.data[i] >= '1' && value.data[i] <= '9') {
    while (i < value.len && value.data[i] >= '0' && value.data[i] <= '9') i++;
  } else {
    return false;
  }
  if (i < value.len && value.data[i] == '.') {
    i++;
    if (i >= value.len || value.data[i] < '0' || value.data[i] > '9') return false;
    while (i < value.len && value.data[i] >= '0' && value.data[i] <= '9') i++;
  }
  if (i < value.len && (value.data[i] == 'e' || value.data[i] == 'E')) {
    i++;
    if (i < value.len && (value.data[i] == '+' || value.data[i] == '-')) i++;
    if (i >= value.len || value.data[i] < '0' || value.data[i] > '9') return false;
    while (i < value.len && value.data[i] >= '0' && value.data[i] <= '9') i++;
  }
  return i == value.len;
}

static bool rt_integer_valid(sp_str_t value, bool sign) {
  u32 i = 0;
  if (i < value.len && value.data[i] == '-') {
    if (!sign) return false;
    i++;
  }
  if (i >= value.len) return false;
  if (value.data[i] == '0') return i + 1 == value.len;
  if (value.data[i] < '1' || value.data[i] > '9') return false;
  while (i < value.len && value.data[i] >= '0' && value.data[i] <= '9') i++;
  return i == value.len;
}

static bool rt_value_coercible(spry_type_t type, sp_str_t value) {
  switch (type) {
    case SPRY_TYPE_STRING: return true;
    case SPRY_TYPE_BOOLEAN: return sp_str_equal_cstr(value, "true") || sp_str_equal_cstr(value, "false");
    case SPRY_TYPE_INT8:
    case SPRY_TYPE_INT16:
    case SPRY_TYPE_INT32: return rt_integer_valid(value, true);
    case SPRY_TYPE_UINT8:
    case SPRY_TYPE_UINT16:
    case SPRY_TYPE_UINT32: return rt_integer_valid(value, false);
    case SPRY_TYPE_FLOAT32:
    case SPRY_TYPE_FLOAT64: return rt_number_valid(value);
  }
  return false;
}

static void rt_out_value(sp_da(c8)* out, spry_type_t type, sp_str_t value) {
  if (type == SPRY_TYPE_STRING) rt_out_json_str(out, value);
  else rt_out_raw(out, value);
}

static sp_str_t rt_fault_json(const spry_fault_t* fault) {
  sp_da(c8) out = sp_da_new(rt_mem(), c8);
  rt_out_raw(&out, sp_str_lit("{\"code\":"));
  rt_out_json_str(&out, sp_cstr_as_str(spry_code_names[fault->code]));
  if (!sp_str_empty(fault->message)) {
    rt_out_raw(&out, sp_str_lit(",\"message\":"));
    rt_out_json_str(&out, fault->message);
  }
  if (sp_da_size(fault->issues)) {
    rt_out_raw(&out, sp_str_lit(",\"issues\":["));
    sp_da_for(fault->issues, i) {
      if (i) sp_da_push(out, ',');
      rt_out_raw(&out, sp_str_lit("{\"path\":"));
      rt_out_json_str(&out, fault->issues[i].path);
      rt_out_raw(&out, sp_str_lit(",\"code\":"));
      rt_out_json_str(&out, fault->issues[i].code);
      sp_da_push(out, '}');
    }
    sp_da_push(out, ']');
  }
  sp_da_push(out, '}');
  return sp_str(out, (u32)sp_da_size(out));
}

static void rt_apply_fault(u32 token, const spry_fault_t* fault) {
  sp_str_t json = rt_fault_json(fault);
  host_report(token, json.data, json.len);
}

static void rt_apply_fault_code(u32 token, spry_code_t code, sp_str_t message) {
  spry_fault_t fault = sp_zero_s(spry_fault_t);
  fault.code = code;
  fault.message = message;
  rt_apply_fault(token, &fault);
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

static bool rt_validate_invoke(const spry_invoke_t* invoke) {
  const spry_endpoint_t* ep = rt_endpoint_find(invoke->handler);
  if (!ep) {
    rt_fatal(sp_fmt(rt_mem(), "unknown endpoint '{}'", sp_fmt_str(invoke->handler)).value);
    return false;
  }
  if (invoke->onResponse == SPRY_ONRESPONSE_PATCH && sp_str_empty(invoke->target)) {
    rt_fatal(sp_fmt(rt_mem(), "endpoint '{}' patches but declares no target", sp_fmt_str(invoke->handler)).value);
    return false;
  }
  sp_da_for(invoke->body, i) {
    const spry_invoke_body_entry_t* entry = &invoke->body[i];
    rt_arg_ref_t ref = rt_endpoint_arg_find(ep, entry->key);
    if (!ref.arg) {
      rt_fatal(sp_fmt(rt_mem(), "endpoint '{}': unknown body argument '{}'", sp_fmt_str(invoke->handler), sp_fmt_str(entry->key)).value);
      return false;
    }
    if (!rt_value_coercible(ref.arg->type, entry->value)) {
      rt_fatal(sp_fmt(rt_mem(), "endpoint '{}': body argument '{}' is not coercible: '{}'", sp_fmt_str(invoke->handler), sp_fmt_str(entry->key), sp_fmt_str(entry->value)).value);
      return false;
    }
  }
  return true;
}

static bool rt_validate_node(const spry_node_t* node) {
  const spry_interaction_t* on = node_on(node);
  if (on) {
    switch (on->kind) {
      case SPRY_INTERACTION_KIND_INVOKE:
        if (!rt_validate_invoke(&on->as.invoke)) return false;
        break;
    }
  }
  if (node->kind == SPRY_NODE_KIND_BOX) {
    sp_da_for(node->as.box.children, i) {
      if (!rt_validate_node(&node->as.box.children[i])) return false;
    }
  }
  return true;
}

static u32 render_node(const spry_node_t* node) {
  u32 handle = host_create_element((u32)node->kind);

  sp_str_t id = node_id(node);
  if (!sp_str_empty(id)) rt_register_id(id, handle);

  const spry_interaction_t* on = node_on(node);
  if (on) {
    switch (on->kind) {
      case SPRY_INTERACTION_KIND_INVOKE: {
        u32 token = rt_push_token(&on->as.invoke);
        host_on_event(handle, (u32)on->as.invoke.event, token);
        break;
      }
    }
  }

  switch (node->kind) {
    case SPRY_NODE_KIND_BOX: {
      const spry_box_t* box = &node->as.box;
      const spry_box_props_t* props = &box->props;
      host_set_direction(handle, (u32)props->direction);
      host_set_gap(handle, props->gap);
      host_set_padding(handle, props->padding);
      host_set_align(handle, (u32)props->align);
      host_set_justify(handle, (u32)props->justify);
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

typedef struct {
  sp_da(c8) body;
  sp_da(spry_fault_issue_t) issues;
  bool first;
} rt_body_builder_t;

static void rt_body_issue(rt_body_builder_t* b, sp_str_t path, sp_str_t code) {
  spry_fault_issue_t issue = { .path = path, .code = code };
  sp_da_push(b->issues, issue);
}

static sp_str_t rt_field_value(u32 field) {
  u32 cap = 1024;
  for (;;) {
    c8* buf = sp_alloc(rt_mem(), cap);
    u32 vlen = host_get_value(field, buf, cap);
    if (vlen < cap) return sp_str(buf, vlen);
    cap *= 2;
  }
}

static void rt_body_arg(rt_body_builder_t* b, const spry_invoke_t* invoke, sp_str_t name, spry_type_t type, bool required) {
  bool from_body = false;
  sp_str_t value = rt_invoke_body_find(invoke, name, &from_body);
  if (!from_body) {
    u32 field = rt_find_field(name);
    if (field == HANDLE_NONE) {
      if (required) rt_body_issue(b, name, sp_str_lit("missing"));
      return;
    }
    value = rt_field_value(field);
  }

  if (!rt_value_coercible(type, value)) {
    rt_body_issue(b, name, sp_str_lit("type"));
    return;
  }

  if (!b->first) sp_da_push(b->body, ',');
  b->first = false;
  rt_out_json_str(&b->body, name);
  sp_da_push(b->body, ':');
  rt_out_value(&b->body, type, value);
}

static bool rt_parse_with(const spry_ast_type_t* type, yyjson_val* val, void* out, spry_ctx_t* ctx) {
  spry_ctx_init(ctx, rt_mem());
  return spry_ast_parse(type, val, ctx, out) == SPRY_OK;
}

static bool rt_parse_node(yyjson_val* val, spry_node_t* out) {
  spry_ctx_t ctx;
  if (!rt_parse_with(&spry_node_type, val, out, &ctx)) {
    rt_fatal(spry_issue_str(rt_mem(), &ctx.issues[0]));
    return false;
  }
  return true;
}

__attribute__((export_name("alloc")))
void* rt_alloc(u32 len) {
  return sp_alloc(rt_mem(), len);
}

__attribute__((export_name("endpoints")))
s32 rt_endpoints(const c8* ptr, u32 len) {
  g_endpoints = SP_NULLPTR;

  yyjson_doc* doc = rt_parse(ptr, len);
  if (!doc) { rt_fatal(sp_str_lit("endpoints parse error")); return 1; }

  spry_endpoints_t endpoints = SP_NULLPTR;
  spry_ctx_t ctx;
  if (!rt_parse_with(&spry_endpoints_type, yyjson_doc_get_root(doc), &endpoints, &ctx)) {
    rt_fatal(spry_issue_str(rt_mem(), &ctx.issues[0]));
    return 2;
  }

  g_endpoints = endpoints;
  return 0;
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
  if (!rt_validate_node(&node)) return 4;

  host_set_root(render_node(&node));
  return 0;
}

__attribute__((export_name("dispatch")))
void rt_dispatch(u32 token) {
  rt_token_t* it = rt_token_at(token);
  if (!it) return;
  if (it->pending) return;

  const spry_invoke_t* invoke = it->invoke;
  const spry_endpoint_t* ep = rt_endpoint_find(invoke->handler);
  if (!ep) {
    rt_fatal(sp_fmt(rt_mem(), "unknown endpoint '{}'", sp_fmt_str(invoke->handler)).value);
    return;
  }

  rt_body_builder_t b = sp_zero_s(rt_body_builder_t);
  b.issues = sp_da_new(rt_mem(), spry_fault_issue_t);
  b.body = sp_da_new(rt_mem(), c8);
  b.first = true;
  sp_da_push(b.body, '{');

  sp_da_for(ep->args.properties, i) {
    rt_body_arg(&b, invoke, ep->args.properties[i].key, ep->args.properties[i].value.type, true);
  }
  sp_da_for(ep->args.optionalProperties, i) {
    rt_body_arg(&b, invoke, ep->args.optionalProperties[i].key, ep->args.optionalProperties[i].value.type, false);
  }
  sp_da_push(b.body, '}');

  sp_da(spry_fault_issue_t) issues = b.issues;
  sp_da(c8) body = b.body;

  if (sp_da_size(issues)) {
    spry_fault_t fault = sp_zero_s(spry_fault_t);
    fault.code = SPRY_CODE_INVALID;
    fault.message = sp_str_lit("argument validation failed");
    fault.issues = issues;
    rt_apply_fault(token, &fault);
    return;
  }

  it->pending = true;
  host_invoke(token, invoke->handler.data, invoke->handler.len, body, (u32)sp_da_size(body));
}

static bool rt_parse_fault(const c8* ptr, u32 len, spry_fault_t* out) {
  yyjson_doc* doc = rt_parse(ptr, len);
  if (!doc) return false;
  spry_ctx_t ctx;
  return rt_parse_with(&spry_fault_type, yyjson_doc_get_root(doc), out, &ctx);
}

static spry_code_t rt_intermediary_code(u32 status) {
  if (status == 408) return SPRY_CODE_TIMEOUT;
  if (status == 429 || status == 502 || status == 503 || status == 504) return SPRY_CODE_UNAVAILABLE;
  return SPRY_CODE_FAILED;
}

static void rt_deliver_ok(rt_token_t* it, u32 token, const c8* ptr, u32 len) {
  (void)token;
  if (it->invoke->onResponse != SPRY_ONRESPONSE_PATCH) return;

  u32 target = rt_lookup_id(it->invoke->target);
  if (target == HANDLE_NONE) { rt_fatal(sp_str_lit("patch target not found")); return; }

  yyjson_doc* doc = rt_parse(ptr, len);
  if (!doc) { rt_fatal(sp_str_lit("fragment parse error")); return; }

  spry_node_t node = sp_zero_s(spry_node_t);
  if (!rt_parse_node(yyjson_doc_get_root(doc), &node)) return;
  if (!rt_validate_node(&node)) return;

  host_clear_children(target);
  host_append_child(target, render_node(&node));
}

__attribute__((export_name("deliver")))
void rt_deliver(u32 token, u32 outcome, const c8* ptr, u32 len) {
  rt_token_t* it = rt_token_at(token);
  if (!it) return;
  if (!it->pending) return;
  it->pending = false;

  if (outcome == DELIVER_OK) {
    rt_deliver_ok(it, token, ptr, len);
    return;
  }

  if (outcome == DELIVER_UNREACHABLE) { rt_apply_fault_code(token, SPRY_CODE_UNAVAILABLE, sp_str_lit("unreachable")); return; }
  if (outcome == DELIVER_TIMEOUT)     { rt_apply_fault_code(token, SPRY_CODE_TIMEOUT, sp_zero_s(sp_str_t)); return; }
  if (outcome == DELIVER_CANCELLED)   { rt_apply_fault_code(token, SPRY_CODE_CANCELLED, sp_zero_s(sp_str_t)); return; }

  if (outcome == DELIVER_FAULT) {
    spry_fault_t fault = sp_zero_s(spry_fault_t);
    if (rt_parse_fault(ptr, len, &fault)) rt_apply_fault(token, &fault);
    else rt_apply_fault_code(token, SPRY_CODE_FAILED, sp_str_lit("malformed fault envelope"));
    return;
  }

  if (outcome >= 100 && outcome <= 599) {
    if (outcome >= 200 && outcome < 300) {
      rt_deliver_ok(it, token, ptr, len);
      return;
    }
    spry_fault_t fault = sp_zero_s(spry_fault_t);
    if (rt_parse_fault(ptr, len, &fault)) {
      rt_apply_fault(token, &fault);
    } else {
      sp_str_t msg = sp_fmt(rt_mem(), "intermediary HTTP {}", sp_fmt_uint(outcome)).value;
      rt_apply_fault_code(token, rt_intermediary_code(outcome), msg);
    }
    return;
  }

  rt_fatal(sp_fmt(rt_mem(), "unknown deliver outcome {}", sp_fmt_uint(outcome)).value);
}

void* malloc(size_t size) {
  return sp_sys_alloc(size);
}

void* realloc(void* ptr, size_t size) {
  (void)ptr; (void)size;
  __builtin_trap();
}

void free(void* ptr) {
  (void)ptr;
}

size_t strlen(const char* s) {
  const char* p = s;
  while (*p) p++;
  return (size_t)(p - s);
}

__int128 __multi3(__int128 a, __int128 b) {
  return a * b;
}
