#include "spry/ast.h"

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
    case SPRY_AST_I32: {
      if (!yyjson_is_int(val)) return spry_issue_code(ctx, SPRY_ERR_AST_EXPECTED_INT);
      s64 n = yyjson_get_sint(val);
      if (n < SP_LIMIT_S32_MIN || n > SP_LIMIT_S32_MAX) {
        return spry_issue(ctx, (spry_issue_t){
          .code = SPRY_ERR_AST_INT_RANGE,
          .num = n, .min = SP_LIMIT_S32_MIN, .max = SP_LIMIT_S32_MAX,
        });
      }
      *(s32*)out = (s32)n;
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

sp_str_t spry_err_name(spry_err_t code) {
  switch (code) {
    case SPRY_OK:                             return sp_str_lit("ok");
    case SPRY_ERR_AST:                        return sp_str_lit("ast");
    case SPRY_ERR_AST_EXPECTED_BOOL:          return sp_str_lit("expected bool");
    case SPRY_ERR_AST_EXPECTED_INT:           return sp_str_lit("expected int");
    case SPRY_ERR_AST_EXPECTED_STR:           return sp_str_lit("expected string");
    case SPRY_ERR_AST_EXPECTED_ENUM:          return sp_str_lit("expected enum string");
    case SPRY_ERR_AST_EXPECTED_ARRAY:         return sp_str_lit("expected array");
    case SPRY_ERR_AST_EXPECTED_OBJECT:        return sp_str_lit("expected object");
    case SPRY_ERR_AST_INT_RANGE:              return sp_str_lit("integer out of range");
    case SPRY_ERR_AST_INVALID_ENUM:           return sp_str_lit("invalid enum value");
    case SPRY_ERR_AST_UNKNOWN_KEY:            return sp_str_lit("unknown key");
    case SPRY_ERR_AST_MISSING_KEY:            return sp_str_lit("missing required key");
    case SPRY_ERR_AST_MISSING_DISCRIMINATOR:  return sp_str_lit("missing discriminator");
    case SPRY_ERR_AST_UNKNOWN_VARIANT:        return sp_str_lit("unknown variant");
  }
  return sp_str_lit("unknown error");
}

sp_str_t spry_issue_str(sp_mem_t mem, const spry_issue_t* issue) {
  switch (issue->code) {
    case SPRY_ERR_AST_INT_RANGE:
      return sp_fmt(mem, "{} at {}: {} not in [{}, {}]", sp_fmt_str(spry_err_name(issue->code)),
                    sp_fmt_str(issue->path), sp_fmt_int(issue->num), sp_fmt_int(issue->min), sp_fmt_int(issue->max)).value;
    case SPRY_ERR_AST_INVALID_ENUM:
    case SPRY_ERR_AST_UNKNOWN_VARIANT:
    case SPRY_ERR_AST_MISSING_DISCRIMINATOR:
      return sp_fmt(mem, "{} '{}' at {}", sp_fmt_str(spry_err_name(issue->code)),
                    sp_fmt_str(issue->detail), sp_fmt_str(issue->path)).value;
    case SPRY_OK:
    case SPRY_ERR_AST:
    case SPRY_ERR_AST_EXPECTED_BOOL:
    case SPRY_ERR_AST_EXPECTED_INT:
    case SPRY_ERR_AST_EXPECTED_STR:
    case SPRY_ERR_AST_EXPECTED_ENUM:
    case SPRY_ERR_AST_EXPECTED_ARRAY:
    case SPRY_ERR_AST_EXPECTED_OBJECT:
    case SPRY_ERR_AST_UNKNOWN_KEY:
    case SPRY_ERR_AST_MISSING_KEY:
      return sp_fmt(mem, "{} at {}", sp_fmt_str(spry_err_name(issue->code)), sp_fmt_str(issue->path)).value;
  }
  return sp_str_lit("unknown error");
}
