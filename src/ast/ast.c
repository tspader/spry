#include "spry/ast.h"

const spry_ast_type_t spry_str_type = { .kind = SPRY_AST_STR };
const spry_ast_type_t spry_bool_type = { .kind = SPRY_AST_BOOL };
const spry_ast_type_t spry_s8_type = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_S8 } };
const spry_ast_type_t spry_u8_type = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_U8 } };
const spry_ast_type_t spry_s16_type = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_S16 } };
const spry_ast_type_t spry_u16_type = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_U16 } };
const spry_ast_type_t spry_s32_type = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_S32 } };
const spry_ast_type_t spry_u32_type = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_U32 } };
const spry_ast_type_t spry_f32_type = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_F32 } };
const spry_ast_type_t spry_f64_type = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_F64 } };

void spry_ctx_init(spry_ctx_t* ctx, sp_mem_t mem) {
  ctx->mem = mem;
  ctx->depth = 0;
  ctx->issues = sp_da_new(mem, spry_issue_t);
}

static void spry_path_push_key(spry_ctx_t* ctx, const c8* key) {
  if (ctx->depth >= SPRY_PATH_MAX) return;
  ctx->path[ctx->depth].kind = SPRY_PATH_KEY;
  ctx->path[ctx->depth].key = key;
  ctx->depth += 1;
}

static void spry_path_push_index(spry_ctx_t* ctx, u32 index) {
  if (ctx->depth >= SPRY_PATH_MAX) return;
  ctx->path[ctx->depth].kind = SPRY_PATH_INDEX;
  ctx->path[ctx->depth].index = index;
  ctx->depth += 1;
}

static void spry_path_pop(spry_ctx_t* ctx) {
  if (ctx->depth) ctx->depth -= 1;
}

static sp_str_t spry_path_join(spry_ctx_t* ctx) {
  sp_str_t out = sp_str_lit("$");
  sp_for(i, ctx->depth) {
    spry_path_seg_t* seg = &ctx->path[i];
    if (seg->kind == SPRY_PATH_KEY) {
      out = sp_fmt(ctx->mem, "{}.{}", sp_fmt_str(out), sp_fmt_cstr(seg->key)).value;
    } else {
      out = sp_fmt(ctx->mem, "{}[{}]", sp_fmt_str(out), sp_fmt_uint(seg->index)).value;
    }
  }
  return out;
}

static spry_err_t spry_issue(spry_ctx_t* ctx, spry_issue_t issue) {
  issue.path = spry_path_join(ctx);
  sp_da_push(ctx->issues, issue);
  return issue.code;
}

static spry_err_t spry_issue_code(spry_ctx_t* ctx, spry_err_t code) {
  return spry_issue(ctx, (spry_issue_t){ .code = code });
}

static sp_str_t spry_json_str(spry_ctx_t* ctx, yyjson_val* val) {
  return sp_str_copy(ctx->mem, sp_str(yyjson_get_str(val), (u32)yyjson_get_len(val)));
}

typedef struct {
  f64  min;
  s64  max;
  bool is_float;
} spry_num_info_t;

static const spry_num_info_t spry_num_info[] = {
  [SPRY_NUM_S8]  = { SP_LIMIT_S8_MIN,  SP_LIMIT_S8_MAX,  false },
  [SPRY_NUM_U8]  = { 0,                SP_LIMIT_U8_MAX,  false },
  [SPRY_NUM_S16] = { SP_LIMIT_S16_MIN, SP_LIMIT_S16_MAX, false },
  [SPRY_NUM_U16] = { 0,                SP_LIMIT_U16_MAX, false },
  [SPRY_NUM_S32] = { SP_LIMIT_S32_MIN, SP_LIMIT_S32_MAX, false },
  [SPRY_NUM_U32] = { 0,                SP_LIMIT_U32_MAX, false },
  [SPRY_NUM_F32] = { 0,                0,                true },
  [SPRY_NUM_F64] = { 0,                0,                true },
};

static void spry_num_store(spry_num_repr_t repr, f64 n, void* out) {
  switch (repr) {
    case SPRY_NUM_S8:  *(s8*)out  = (s8)n;  return;
    case SPRY_NUM_U8:  *(u8*)out  = (u8)n;  return;
    case SPRY_NUM_S16: *(s16*)out = (s16)n; return;
    case SPRY_NUM_U16: *(u16*)out = (u16)n; return;
    case SPRY_NUM_S32: *(s32*)out = (s32)n; return;
    case SPRY_NUM_U32: *(u32*)out = (u32)n; return;
    case SPRY_NUM_F32: *(f32*)out = (f32)n; return;
    case SPRY_NUM_F64: *(f64*)out = n;      return;
  }
}

static spry_err_t spry_ast_parse_object(const spry_ast_object_t* obj, yyjson_val* val, spry_ctx_t* ctx, void* out, const c8* skip) {
  if (!yyjson_is_obj(val)) return spry_issue_code(ctx, SPRY_ERR_AST_EXPECTED_OBJECT);

  spry_err_t first = SPRY_OK;

  size_t idx, max;
  yyjson_val* key;
  yyjson_val* value;
  yyjson_obj_foreach(val, idx, max, key, value) {
    const c8* name = yyjson_get_str(key);
    if (skip && sp_cstr_equal(name, skip)) continue;

    const spry_ast_field_t* field = SP_NULLPTR;
    sp_for(i, obj->count) {
      if (sp_cstr_equal(obj->fields[i].key, name)) { field = &obj->fields[i]; break; }
    }
    if (!field) {
      spry_path_push_key(ctx, name);
      spry_err_t code = spry_issue(ctx, (spry_issue_t){
        .code = SPRY_ERR_AST_UNKNOWN_KEY,
        .detail = sp_str_copy(ctx->mem, sp_cstr_as_str(name)),
      });
      spry_path_pop(ctx);
      if (!first) first = code;
      continue;
    }

    void* slot = (u8*)out + field->offset;
    void* dst = slot;
    if (field->is_ptr) {
      dst = sp_alloc(ctx->mem, field->size);
      *(void**)slot = dst;
    }

    spry_path_push_key(ctx, field->key);
    spry_err_t code = spry_ast_parse(field->type, value, ctx, dst);
    spry_path_pop(ctx);
    if (code && !first) first = code;
  }

  sp_for(i, obj->count) {
    const spry_ast_field_t* field = &obj->fields[i];
    if (field->required && !yyjson_obj_get(val, field->key)) {
      spry_path_push_key(ctx, field->key);
      spry_err_t code = spry_issue(ctx, (spry_issue_t){
        .code = SPRY_ERR_AST_MISSING_KEY,
        .detail = sp_str_copy(ctx->mem, sp_cstr_as_str(field->key)),
      });
      spry_path_pop(ctx);
      if (!first) first = code;
    }
  }

  return first;
}

spry_err_t spry_ast_parse(const spry_ast_type_t* type, yyjson_val* val, spry_ctx_t* ctx, void* out) {
  switch (type->kind) {
    case SPRY_AST_BOOL: {
      if (!yyjson_is_bool(val)) return spry_issue_code(ctx, SPRY_ERR_AST_EXPECTED_BOOL);
      *(bool*)out = yyjson_get_bool(val);
      return SPRY_OK;
    }
    case SPRY_AST_NUMBER: {
      if (!yyjson_is_num(val)) return spry_issue_code(ctx, SPRY_ERR_AST_EXPECTED_NUMBER);
      f64 n = yyjson_get_num(val);
      spry_num_repr_t repr = type->as.number.repr;
      spry_num_info_t info = spry_num_info[repr];
      if (!info.is_float) {
        if (n < info.min || n > (f64)info.max) {
          return spry_issue(ctx, (spry_issue_t){
            .code = SPRY_ERR_AST_INT_RANGE,
            .num = n, .min = (s64)info.min, .max = info.max,
          });
        }
        if ((f64)(s64)n != n) {
          return spry_issue(ctx, (spry_issue_t){ .code = SPRY_ERR_AST_INT_FRACTIONAL, .num = n });
        }
      }
      spry_num_store(repr, n, out);
      return SPRY_OK;
    }
    case SPRY_AST_STR: {
      if (!yyjson_is_str(val)) return spry_issue_code(ctx, SPRY_ERR_AST_EXPECTED_STR);
      *(sp_str_t*)out = spry_json_str(ctx, val);
      return SPRY_OK;
    }
    case SPRY_AST_ENUM: {
      if (!yyjson_is_str(val)) return spry_issue_code(ctx, SPRY_ERR_AST_EXPECTED_ENUM);
      sp_str_t s = sp_str(yyjson_get_str(val), (u32)yyjson_get_len(val));
      sp_for(i, type->as.enom.count) {
        if (sp_str_equal_cstr(s, type->as.enom.names[i])) {
          *(s32*)out = (s32)i;
          return SPRY_OK;
        }
      }
      return spry_issue(ctx, (spry_issue_t){
        .code = SPRY_ERR_AST_INVALID_ENUM,
        .detail = sp_str_copy(ctx->mem, s),
      });
    }
    case SPRY_AST_ARRAY: {
      if (!yyjson_is_arr(val)) return spry_issue_code(ctx, SPRY_ERR_AST_EXPECTED_ARRAY);
      u32 stride = type->as.array.stride;
      void* arr = sp_da_init_ex(ctx->mem, stride);
      spry_err_t first = SPRY_OK;
      size_t idx, max;
      yyjson_val* elem;
      yyjson_arr_foreach(val, idx, max, elem) {
        arr = sp_da_grow_ex(arr, stride, 1);
        sp_da_header_t* head = sp_da_head(arr);
        void* slot = (u8*)arr + head->size * stride;
        head->size += 1;
        sp_mem_zero(slot, stride);
        spry_path_push_index(ctx, (u32)idx);
        spry_err_t code = spry_ast_parse(type->as.array.element, elem, ctx, slot);
        spry_path_pop(ctx);
        if (code && !first) first = code;
      }
      *(void**)out = arr;
      return first;
    }
    case SPRY_AST_VALUES: {
      if (!yyjson_is_obj(val)) return spry_issue_code(ctx, SPRY_ERR_AST_EXPECTED_OBJECT);
      const spry_ast_values_t* values = &type->as.values;
      u32 stride = values->stride;
      void* arr = sp_da_init_ex(ctx->mem, stride);
      spry_err_t first = SPRY_OK;
      size_t idx, max;
      yyjson_val* key;
      yyjson_val* elem;
      yyjson_obj_foreach(val, idx, max, key, elem) {
        arr = sp_da_grow_ex(arr, stride, 1);
        sp_da_header_t* head = sp_da_head(arr);
        void* slot = (u8*)arr + head->size * stride;
        head->size += 1;
        sp_mem_zero(slot, stride);
        *(sp_str_t*)((u8*)slot + values->key_offset) = spry_json_str(ctx, key);
        spry_path_push_key(ctx, yyjson_get_str(key));
        spry_err_t code = spry_ast_parse(values->value, elem, ctx, (u8*)slot + values->value_offset);
        spry_path_pop(ctx);
        if (code && !first) first = code;
      }
      *(void**)out = arr;
      return first;
    }
    case SPRY_AST_OBJECT: {
      return spry_ast_parse_object(&type->as.object, val, ctx, out, SP_NULLPTR);
    }
    case SPRY_AST_UNION: {
      const spry_ast_union_t* uni = &type->as.uni;
      if (!yyjson_is_obj(val)) return spry_issue_code(ctx, SPRY_ERR_AST_EXPECTED_OBJECT);

      yyjson_val* tag = yyjson_obj_get(val, uni->tag_key);
      if (!tag || !yyjson_is_str(tag)) {
        return spry_issue(ctx, (spry_issue_t){
          .code = SPRY_ERR_AST_MISSING_DISCRIMINATOR,
          .detail = sp_str_copy(ctx->mem, sp_cstr_as_str(uni->tag_key)),
        });
      }

      sp_str_t tagv = sp_str(yyjson_get_str(tag), (u32)yyjson_get_len(tag));
      const spry_ast_variant_t* variant = SP_NULLPTR;
      sp_for(i, uni->count) {
        if (sp_str_equal_cstr(tagv, uni->variants[i].tag)) { variant = &uni->variants[i]; break; }
      }
      if (!variant) {
        return spry_issue(ctx, (spry_issue_t){
          .code = SPRY_ERR_AST_UNKNOWN_VARIANT,
          .detail = sp_str_copy(ctx->mem, tagv),
        });
      }

      *(s32*)((u8*)out + uni->tag_offset) = variant->value;
      return spry_ast_parse_object(&variant->type->as.object, val, ctx, (u8*)out + uni->payload_offset, uni->tag_key);
    }
  }
  return SPRY_OK;
}

void spry_json_push(sp_da(c8)* out, sp_str_t raw) {
  sp_for(i, raw.len) sp_da_push(*out, raw.data[i]);
}

void spry_json_push_str(sp_da(c8)* out, sp_str_t value) {
  static const c8 hex[] = "0123456789abcdef";
  sp_da_push(*out, '"');
  sp_for(i, value.len) {
    c8 c = value.data[i];
    if (c == '"' || c == '\\') {
      sp_da_push(*out, '\\');
      sp_da_push(*out, c);
    } else if ((u8)c < 0x20) {
      spry_json_push(out, sp_str_lit("\\u00"));
      sp_da_push(*out, hex[((u8)c >> 4) & 0xf]);
      sp_da_push(*out, hex[(u8)c & 0xf]);
    } else {
      sp_da_push(*out, c);
    }
  }
  sp_da_push(*out, '"');
}

static bool spry_ast_would_write(const spry_ast_type_t* type, const void* value);

static bool spry_ast_field_present(const spry_ast_field_t* field, const void* obj) {
  const void* slot = (const u8*)obj + field->offset;
  if (field->is_ptr) return *(void* const*)slot != SP_NULLPTR;
  if (field->required) return true;
  return spry_ast_would_write(field->type, slot);
}

static bool spry_ast_would_write(const spry_ast_type_t* type, const void* value) {
  switch (type->kind) {
    case SPRY_AST_BOOL: return *(const bool*)value;
    case SPRY_AST_NUMBER: {
      switch (type->as.number.repr) {
        case SPRY_NUM_S8:  return *(const s8*)value != 0;
        case SPRY_NUM_U8:  return *(const u8*)value != 0;
        case SPRY_NUM_S16: return *(const s16*)value != 0;
        case SPRY_NUM_U16: return *(const u16*)value != 0;
        case SPRY_NUM_S32: return *(const s32*)value != 0;
        case SPRY_NUM_U32: return *(const u32*)value != 0;
        case SPRY_NUM_F32: return *(const f32*)value != 0;
        case SPRY_NUM_F64: return *(const f64*)value != 0;
      }
      return false;
    }
    case SPRY_AST_STR: return !sp_str_empty(*(const sp_str_t*)value);
    case SPRY_AST_ENUM: return *(const s32*)value != 0;
    case SPRY_AST_ARRAY:
    case SPRY_AST_VALUES: return sp_da_size(*(void* const*)value) > 0;
    case SPRY_AST_OBJECT: {
      sp_for(i, type->as.object.count) {
        if (spry_ast_field_present(&type->as.object.fields[i], value)) return true;
      }
      return false;
    }
    case SPRY_AST_UNION: return true;
  }
  return false;
}

static sp_str_t spry_float_str(sp_mem_t mem, f64 value) {
  sp_str_t s = sp_fmt(mem, "{}", sp_fmt_float(value)).value;
  bool fractional = false;
  sp_for(i, s.len) {
    if (s.data[i] == '.') { fractional = true; break; }
  }
  if (!fractional) return s;
  u32 len = s.len;
  while (len && s.data[len - 1] == '0') len--;
  if (len && s.data[len - 1] == '.') len--;
  return sp_str(s.data, len);
}

static void spry_ast_write_value(sp_mem_t mem, const spry_ast_type_t* type, const void* value, sp_da(c8)* out);

static void spry_ast_write_fields(sp_mem_t mem, const spry_ast_object_t* obj, const void* value, sp_da(c8)* out, bool* first) {
  sp_for(i, obj->count) {
    const spry_ast_field_t* field = &obj->fields[i];
    if (!spry_ast_field_present(field, value)) continue;
    const void* slot = (const u8*)value + field->offset;
    if (field->is_ptr) slot = *(void* const*)slot;
    if (!*first) sp_da_push(*out, ',');
    *first = false;
    spry_json_push_str(out, sp_cstr_as_str(field->key));
    sp_da_push(*out, ':');
    spry_ast_write_value(mem, field->type, slot, out);
  }
}

static void spry_ast_write_value(sp_mem_t mem, const spry_ast_type_t* type, const void* value, sp_da(c8)* out) {
  switch (type->kind) {
    case SPRY_AST_BOOL: {
      spry_json_push(out, *(const bool*)value ? sp_str_lit("true") : sp_str_lit("false"));
      return;
    }
    case SPRY_AST_NUMBER: {
      switch (type->as.number.repr) {
        case SPRY_NUM_S8:  spry_json_push(out, sp_fmt(mem, "{}", sp_fmt_int(*(const s8*)value)).value); return;
        case SPRY_NUM_U8:  spry_json_push(out, sp_fmt(mem, "{}", sp_fmt_uint(*(const u8*)value)).value); return;
        case SPRY_NUM_S16: spry_json_push(out, sp_fmt(mem, "{}", sp_fmt_int(*(const s16*)value)).value); return;
        case SPRY_NUM_U16: spry_json_push(out, sp_fmt(mem, "{}", sp_fmt_uint(*(const u16*)value)).value); return;
        case SPRY_NUM_S32: spry_json_push(out, sp_fmt(mem, "{}", sp_fmt_int(*(const s32*)value)).value); return;
        case SPRY_NUM_U32: spry_json_push(out, sp_fmt(mem, "{}", sp_fmt_uint(*(const u32*)value)).value); return;
        case SPRY_NUM_F32: spry_json_push(out, spry_float_str(mem, *(const f32*)value)); return;
        case SPRY_NUM_F64: spry_json_push(out, spry_float_str(mem, *(const f64*)value)); return;
      }
      return;
    }
    case SPRY_AST_STR: {
      spry_json_push_str(out, *(const sp_str_t*)value);
      return;
    }
    case SPRY_AST_ENUM: {
      spry_json_push_str(out, sp_cstr_as_str(type->as.enom.names[*(const s32*)value]));
      return;
    }
    case SPRY_AST_ARRAY: {
      const void* arr = *(void* const*)value;
      sp_da_push(*out, '[');
      sp_for(i, sp_da_size(arr)) {
        if (i) sp_da_push(*out, ',');
        spry_ast_write_value(mem, type->as.array.element, (const u8*)arr + i * type->as.array.stride, out);
      }
      sp_da_push(*out, ']');
      return;
    }
    case SPRY_AST_VALUES: {
      const spry_ast_values_t* values = &type->as.values;
      const void* arr = *(void* const*)value;
      sp_da_push(*out, '{');
      sp_for(i, sp_da_size(arr)) {
        const u8* slot = (const u8*)arr + i * values->stride;
        if (i) sp_da_push(*out, ',');
        spry_json_push_str(out, *(const sp_str_t*)(slot + values->key_offset));
        sp_da_push(*out, ':');
        spry_ast_write_value(mem, values->value, slot + values->value_offset, out);
      }
      sp_da_push(*out, '}');
      return;
    }
    case SPRY_AST_OBJECT: {
      sp_da_push(*out, '{');
      bool first = true;
      spry_ast_write_fields(mem, &type->as.object, value, out, &first);
      sp_da_push(*out, '}');
      return;
    }
    case SPRY_AST_UNION: {
      const spry_ast_union_t* uni = &type->as.uni;
      s32 tag = *(const s32*)((const u8*)value + uni->tag_offset);
      const spry_ast_variant_t* variant = SP_NULLPTR;
      sp_for(i, uni->count) {
        if (uni->variants[i].value == tag) { variant = &uni->variants[i]; break; }
      }
      SP_ASSERT(variant);
      sp_da_push(*out, '{');
      spry_json_push_str(out, sp_cstr_as_str(uni->tag_key));
      sp_da_push(*out, ':');
      spry_json_push_str(out, sp_cstr_as_str(variant->tag));
      bool first = false;
      spry_ast_write_fields(mem, &variant->type->as.object, (const u8*)value + uni->payload_offset, out, &first);
      sp_da_push(*out, '}');
      return;
    }
  }
}

sp_str_t spry_ast_write(sp_mem_t mem, const spry_ast_type_t* type, const void* value) {
  sp_da(c8) out = sp_da_new(mem, c8);
  spry_ast_write_value(mem, type, value, &out);
  return sp_str(out, (u32)sp_da_size(out));
}

sp_str_t spry_err_name(spry_err_t code) {
  switch (code) {
    case SPRY_OK:                             return sp_str_lit("ok");
    case SPRY_ERR_AST:                        return sp_str_lit("ast");
    case SPRY_ERR_AST_EXPECTED_BOOL:          return sp_str_lit("expected bool");
    case SPRY_ERR_AST_EXPECTED_NUMBER:        return sp_str_lit("expected number");
    case SPRY_ERR_AST_EXPECTED_STR:           return sp_str_lit("expected string");
    case SPRY_ERR_AST_EXPECTED_ENUM:          return sp_str_lit("expected enum string");
    case SPRY_ERR_AST_EXPECTED_ARRAY:         return sp_str_lit("expected array");
    case SPRY_ERR_AST_EXPECTED_OBJECT:        return sp_str_lit("expected object");
    case SPRY_ERR_AST_INT_RANGE:              return sp_str_lit("integer out of range");
    case SPRY_ERR_AST_INT_FRACTIONAL:         return sp_str_lit("integer has fractional part");
    case SPRY_ERR_AST_INVALID_ENUM:           return sp_str_lit("invalid enum value");
    case SPRY_ERR_AST_UNKNOWN_KEY:            return sp_str_lit("unknown key");
    case SPRY_ERR_AST_MISSING_KEY:            return sp_str_lit("missing required key");
    case SPRY_ERR_AST_MISSING_DISCRIMINATOR:  return sp_str_lit("missing discriminator");
    case SPRY_ERR_AST_UNKNOWN_VARIANT:        return sp_str_lit("unknown variant");
    case SPRY_ERR_JSON:                       return sp_str_lit("invalid json");
    case SPRY_ERR_GEN_PREFIX_MISSING:         return sp_str_lit("prefix is required");
    case SPRY_ERR_GEN_PREFIX_INVALID:         return sp_str_lit("invalid prefix");
    case SPRY_ERR_GEN_ENDPOINT_INVALID:       return sp_str_lit("invalid endpoint name");
    case SPRY_ERR_GEN_ENDPOINT_DUP:           return sp_str_lit("duplicate endpoint");
    case SPRY_ERR_GEN_ARG_INVALID:            return sp_str_lit("invalid argument name");
    case SPRY_ERR_GEN_ARG_RESERVED:           return sp_str_lit("reserved argument name");
    case SPRY_ERR_GEN_ARG_DUP:                return sp_str_lit("duplicate argument");
    case SPRY_ERR_GEN_ARG_TYPE:               return sp_str_lit("unsupported argument type");
    case SPRY_ERR_GEN_WRITE:                  return sp_str_lit("write failed");
  }
  return sp_str_lit("unknown error");
}

sp_str_t spry_issue_str(sp_mem_t mem, const spry_issue_t* issue) {
  switch (issue->code) {
    case SPRY_ERR_AST_INT_RANGE:
      return sp_fmt(mem, "{} at {}: {} not in [{}, {}]", sp_fmt_str(spry_err_name(issue->code)),
                    sp_fmt_str(issue->path), sp_fmt_float(issue->num), sp_fmt_int(issue->min), sp_fmt_int(issue->max)).value;
    case SPRY_ERR_AST_INT_FRACTIONAL:
      return sp_fmt(mem, "{} at {}: {}", sp_fmt_str(spry_err_name(issue->code)),
                    sp_fmt_str(issue->path), sp_fmt_float(issue->num)).value;
    case SPRY_ERR_AST_INVALID_ENUM:
    case SPRY_ERR_AST_UNKNOWN_VARIANT:
    case SPRY_ERR_AST_MISSING_DISCRIMINATOR:
      return sp_fmt(mem, "{} '{}' at {}", sp_fmt_str(spry_err_name(issue->code)),
                    sp_fmt_str(issue->detail), sp_fmt_str(issue->path)).value;
    default:
      return sp_fmt(mem, "{} at {}", sp_fmt_str(spry_err_name(issue->code)), sp_fmt_str(issue->path)).value;
  }
}
