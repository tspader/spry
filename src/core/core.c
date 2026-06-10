#include "spry/core.h"

spry_err_t spry_endpoints_parse_val(sp_mem_t mem, yyjson_val* val, spry_endpoints_t* out, spry_issue_t* issue) {
  spry_ctx_t ctx;
  spry_ctx_init(&ctx, mem);
  spry_err_t err = spry_ast_parse(&spry_endpoints_type, val, &ctx, out);
  if (err && issue) *issue = ctx.issues[0];
  return err;
}

spry_err_t spry_endpoints_parse(sp_mem_t mem, sp_str_t json, spry_endpoints_t* out, spry_issue_t* issue) {
  *out = SP_NULLPTR;
  yyjson_doc* doc = yyjson_read(json.data, json.len, 0);
  if (!doc) {
    if (issue) *issue = (spry_issue_t){ .code = SPRY_ERR_JSON, .path = sp_str_lit("$") };
    return SPRY_ERR_JSON;
  }
  spry_err_t err = spry_endpoints_parse_val(mem, yyjson_doc_get_root(doc), out, issue);
  yyjson_doc_free(doc);
  return err;
}

const spry_endpoint_t* spry_endpoint_find(spry_endpoints_t endpoints, sp_str_t name) {
  sp_da_for(endpoints, i) {
    if (sp_str_equal(endpoints[i].key, name)) return &endpoints[i].value;
  }
  return SP_NULLPTR;
}

const spry_endpoint_arg_t* spry_endpoint_arg_find(const spry_endpoint_t* ep, sp_str_t name) {
  sp_da_for(ep->args.properties, i) {
    if (sp_str_equal(ep->args.properties[i].key, name)) return &ep->args.properties[i].value;
  }
  sp_da_for(ep->args.optionalProperties, i) {
    if (sp_str_equal(ep->args.optionalProperties[i].key, name)) return &ep->args.optionalProperties[i].value;
  }
  return SP_NULLPTR;
}

typedef struct {
  s64 min;
  s64 max;
} spry_int_range_t;

static spry_int_range_t spry_type_range(spry_type_t type) {
  switch (type) {
    case SPRY_TYPE_INT8:   return (spry_int_range_t){ SP_LIMIT_S8_MIN,  SP_LIMIT_S8_MAX };
    case SPRY_TYPE_INT16:  return (spry_int_range_t){ SP_LIMIT_S16_MIN, SP_LIMIT_S16_MAX };
    case SPRY_TYPE_INT32:  return (spry_int_range_t){ SP_LIMIT_S32_MIN, SP_LIMIT_S32_MAX };
    case SPRY_TYPE_UINT8:  return (spry_int_range_t){ 0, SP_LIMIT_U8_MAX };
    case SPRY_TYPE_UINT16: return (spry_int_range_t){ 0, SP_LIMIT_U16_MAX };
    case SPRY_TYPE_UINT32: return (spry_int_range_t){ 0, SP_LIMIT_U32_MAX };
    case SPRY_TYPE_STRING: break;
    case SPRY_TYPE_BOOLEAN: break;
    case SPRY_TYPE_FLOAT32: break;
    case SPRY_TYPE_FLOAT64: break;
  }
  return sp_zero_s(spry_int_range_t);
}

static bool spry_integer_in_range(sp_str_t value, spry_int_range_t range) {
  bool neg = value.data[0] == '-';
  s64 n = 0;
  for (u32 i = neg ? 1 : 0; i < value.len; i++) {
    s64 digit = value.data[i] - '0';
    if (n > (SP_LIMIT_S64_MAX - digit) / 10) return false;
    n = n * 10 + digit;
  }
  if (neg) n = -n;
  return n >= range.min && n <= range.max;
}

static bool spry_number_grammar(sp_str_t value) {
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

static bool spry_integer_grammar(sp_str_t value, bool sign) {
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

bool spry_coercible(spry_type_t type, sp_str_t value) {
  switch (type) {
    case SPRY_TYPE_STRING: return true;
    case SPRY_TYPE_BOOLEAN: return sp_str_equal_cstr(value, "true") || sp_str_equal_cstr(value, "false");
    case SPRY_TYPE_INT8:
    case SPRY_TYPE_INT16:
    case SPRY_TYPE_INT32: return spry_integer_grammar(value, true) && spry_integer_in_range(value, spry_type_range(type));
    case SPRY_TYPE_UINT8:
    case SPRY_TYPE_UINT16:
    case SPRY_TYPE_UINT32: return spry_integer_grammar(value, false) && spry_integer_in_range(value, spry_type_range(type));
    case SPRY_TYPE_FLOAT32:
    case SPRY_TYPE_FLOAT64: return spry_number_grammar(value);
  }
  return false;
}

static void spry_issue_push(sp_mem_t mem, spry_issues_t* issues, sp_str_t path, sp_str_t code) {
  spry_fault_issue_t issue = { .path = sp_str_copy(mem, path), .code = code };
  sp_da_push(*issues, issue);
}

static bool spry_int_val_ok(yyjson_val* val, spry_int_range_t range) {
  if (yyjson_is_sint(val)) {
    s64 n = yyjson_get_sint(val);
    return n >= range.min && n <= range.max;
  }
  if (yyjson_is_uint(val)) {
    u64 n = yyjson_get_uint(val);
    return n <= (u64)range.max;
  }
  return false;
}

static bool spry_arg_type_ok(spry_type_t type, yyjson_val* val) {
  switch (type) {
    case SPRY_TYPE_STRING: return yyjson_is_str(val);
    case SPRY_TYPE_BOOLEAN: return yyjson_is_bool(val);
    case SPRY_TYPE_INT8:
    case SPRY_TYPE_INT16:
    case SPRY_TYPE_INT32:
    case SPRY_TYPE_UINT8:
    case SPRY_TYPE_UINT16:
    case SPRY_TYPE_UINT32: return spry_int_val_ok(val, spry_type_range(type));
    case SPRY_TYPE_FLOAT32:
    case SPRY_TYPE_FLOAT64: return yyjson_is_num(val);
  }
  return false;
}

spry_issues_t spry_validate_args(sp_mem_t mem, const spry_endpoint_t* ep, yyjson_val* body) {
  spry_issues_t issues = sp_da_new(mem, spry_fault_issue_t);
  if (!yyjson_is_obj(body)) {
    spry_issue_push(mem, &issues, sp_str_lit("$"), sp_str_lit("type"));
    return issues;
  }
  size_t idx, max;
  yyjson_val* key;
  yyjson_val* value;
  yyjson_obj_foreach(body, idx, max, key, value) {
    sp_str_t name = sp_str(yyjson_get_str(key), (u32)yyjson_get_len(key));
    const spry_endpoint_arg_t* arg = spry_endpoint_arg_find(ep, name);
    if (!arg) {
      spry_issue_push(mem, &issues, name, sp_str_lit("unknown"));
      continue;
    }
    if (!spry_arg_type_ok(arg->type, value)) {
      spry_issue_push(mem, &issues, name, sp_str_lit("type"));
    }
  }
  sp_da_for(ep->args.properties, i) {
    sp_str_t name = ep->args.properties[i].key;
    if (!yyjson_obj_getn(body, name.data, name.len)) {
      spry_issue_push(mem, &issues, name, sp_str_lit("missing"));
    }
  }
  return issues;
}

sp_str_t spry_fault_write(sp_mem_t mem, const spry_fault_t* fault) {
  return spry_ast_write(mem, &spry_fault_type, fault);
}

bool spry_fault_parse_val(sp_mem_t mem, yyjson_val* val, spry_fault_t* out) {
  spry_ctx_t ctx;
  spry_ctx_init(&ctx, mem);
  return spry_ast_parse(&spry_fault_type, val, &ctx, out) == SPRY_OK;
}

bool spry_code_parse(sp_str_t name, spry_code_t* out) {
  sp_carr_for(spry_code_names, i) {
    if (sp_str_equal_cstr(name, spry_code_names[i])) {
      *out = (spry_code_t)i;
      return true;
    }
  }
  return false;
}

u16 spry_fault_status(spry_code_t code) {
  switch (code) {
    case SPRY_CODE_INVALID: return 422;
    case SPRY_CODE_UNAUTHENTICATED: return 401;
    case SPRY_CODE_DENIED: return 403;
    case SPRY_CODE_MISSING: return 404;
    case SPRY_CODE_CONFLICT: return 409;
    case SPRY_CODE_FAILED: return 500;
    case SPRY_CODE_UNAVAILABLE: return 503;
    case SPRY_CODE_TIMEOUT: return 408;
    case SPRY_CODE_CANCELLED: return 0;
  }
  return 0;
}

spry_code_t spry_intermediary_code(u32 status) {
  if (status == 408) return SPRY_CODE_TIMEOUT;
  if (status == 429 || status == 502 || status == 503 || status == 504) return SPRY_CODE_UNAVAILABLE;
  return SPRY_CODE_FAILED;
}

bool spry_node_parse_val(sp_mem_t mem, yyjson_val* val, spry_node_t* out, sp_str_t* error) {
  spry_ctx_t ctx;
  spry_ctx_init(&ctx, mem);
  if (spry_ast_parse(&spry_node_type, val, &ctx, out) != SPRY_OK) {
    if (error) *error = spry_issue_str(mem, &ctx.issues[0]);
    return false;
  }
  return true;
}

bool spry_node_parse(sp_mem_t mem, sp_str_t json, spry_node_t* out, sp_str_t* error) {
  *out = sp_zero_s(spry_node_t);
  yyjson_doc* doc = yyjson_read(json.data, json.len, 0);
  if (!doc) {
    if (error) *error = sp_str_lit("fragment json parse error");
    return false;
  }
  bool ok = spry_node_parse_val(mem, yyjson_doc_get_root(doc), out, error);
  yyjson_doc_free(doc);
  return ok;
}

sp_str_t spry_node_id(const spry_node_t* node) {
  switch (node->kind) {
    case SPRY_NODE_KIND_BOX:    return node->as.box.id;
    case SPRY_NODE_KIND_TEXT:   return node->as.text.id;
    case SPRY_NODE_KIND_LINK:   return node->as.link.id;
    case SPRY_NODE_KIND_INPUT:  return node->as.input.id;
    case SPRY_NODE_KIND_BUTTON: return node->as.button.id;
  }
  return sp_zero_s(sp_str_t);
}

const spry_interaction_t* spry_node_on(const spry_node_t* node) {
  switch (node->kind) {
    case SPRY_NODE_KIND_BOX:    return node->as.box.on;
    case SPRY_NODE_KIND_BUTTON: return node->as.button.on;
    case SPRY_NODE_KIND_TEXT:   return SP_NULLPTR;
    case SPRY_NODE_KIND_LINK:   return SP_NULLPTR;
    case SPRY_NODE_KIND_INPUT:  return SP_NULLPTR;
  }
  return SP_NULLPTR;
}
