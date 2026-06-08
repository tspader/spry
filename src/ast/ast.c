#include "spry/ast.h"

static bool spry_ast_fail(sp_str_t* err, sp_str_t msg) {
  if (err) *err = msg;
  return false;
}

static bool spry_ast_parse_object(const spry_ast_object_t* obj, yyjson_val* val, sp_mem_t mem, void* out, const c8* skip, sp_str_t* err) {
  if (!yyjson_is_obj(val)) return spry_ast_fail(err, sp_str_lit("expected object"));

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
    if (!field) return spry_ast_fail(err, sp_fmt(mem, "unknown key '{}'", sp_fmt_cstr(name)).value);

    void* slot = (u8*)out + field->offset;
    void* dst = slot;
    if (field->is_ptr) {
      dst = sp_alloc(mem, field->size);
      *(void**)slot = dst;
    }
    if (!spry_ast_parse(field->type, value, mem, dst, err)) return false;
  }

  sp_for(i, obj->count) {
    const spry_ast_field_t* field = &obj->fields[i];
    if (field->required && !yyjson_obj_get(val, field->key)) {
      return spry_ast_fail(err, sp_fmt(mem, "missing required key '{}'", sp_fmt_cstr(field->key)).value);
    }
  }
  return true;
}

bool spry_ast_parse(const spry_ast_type_t* type, yyjson_val* val, sp_mem_t mem, void* out, sp_str_t* err) {
  switch (type->kind) {
    case SPRY_AST_BOOL: {
      if (!yyjson_is_bool(val)) return spry_ast_fail(err, sp_str_lit("expected bool"));
      *(bool*)out = yyjson_get_bool(val);
      return true;
    }
    case SPRY_AST_I32: {
      if (!yyjson_is_int(val)) return spry_ast_fail(err, sp_str_lit("expected integer"));
      s64 n = yyjson_get_sint(val);
      if (n < SP_LIMIT_S32_MIN || n > SP_LIMIT_S32_MAX) return spry_ast_fail(err, sp_str_lit("integer out of range"));
      *(s32*)out = (s32)n;
      return true;
    }
    case SPRY_AST_STR: {
      if (!yyjson_is_str(val)) return spry_ast_fail(err, sp_str_lit("expected string"));
      sp_str_t s = sp_str(yyjson_get_str(val), (u32)yyjson_get_len(val));
      *(sp_str_t*)out = sp_str_copy(mem, s);
      return true;
    }
    case SPRY_AST_ENUM: {
      if (!yyjson_is_str(val)) return spry_ast_fail(err, sp_str_lit("expected enum string"));
      sp_str_t s = sp_str(yyjson_get_str(val), (u32)yyjson_get_len(val));
      sp_for(i, type->as.enom.count) {
        if (sp_str_equal_cstr(s, type->as.enom.names[i])) {
          *(s32*)out = (s32)i;
          return true;
        }
      }
      return spry_ast_fail(err, sp_fmt(mem, "invalid enum value '{}'", sp_fmt_str(s)).value);
    }
    case SPRY_AST_ARRAY: {
      if (!yyjson_is_arr(val)) return spry_ast_fail(err, sp_str_lit("expected array"));
      u32 stride = type->as.array.stride;
      void* arr = sp_da_init_ex(mem, stride);
      size_t idx, max;
      yyjson_val* elem;
      yyjson_arr_foreach(val, idx, max, elem) {
        arr = sp_da_grow_ex(arr, stride, 1);
        sp_da_header_t* head = sp_da_head(arr);
        void* slot = (u8*)arr + head->size * stride;
        head->size += 1;
        sp_mem_zero(slot, stride);
        if (!spry_ast_parse(type->as.array.element, elem, mem, slot, err)) return false;
      }
      *(void**)out = arr;
      return true;
    }
    case SPRY_AST_OBJECT: {
      return spry_ast_parse_object(&type->as.object, val, mem, out, SP_NULLPTR, err);
    }
    case SPRY_AST_UNION: {
      const spry_ast_union_t* uni = &type->as.uni;
      if (!yyjson_is_obj(val)) return spry_ast_fail(err, sp_str_lit("expected object"));

      yyjson_val* tag = yyjson_obj_get(val, uni->tag_key);
      if (!tag || !yyjson_is_str(tag)) {
        return spry_ast_fail(err, sp_fmt(mem, "missing discriminator '{}'", sp_fmt_cstr(uni->tag_key)).value);
      }

      sp_str_t tagv = sp_str(yyjson_get_str(tag), (u32)yyjson_get_len(tag));
      const spry_ast_variant_t* variant = SP_NULLPTR;
      sp_for(i, uni->count) {
        if (sp_str_equal_cstr(tagv, uni->variants[i].tag)) { variant = &uni->variants[i]; break; }
      }
      if (!variant) return spry_ast_fail(err, sp_fmt(mem, "unknown variant '{}'", sp_fmt_str(tagv)).value);

      *(s32*)((u8*)out + uni->tag_offset) = variant->value;
      return spry_ast_parse_object(&variant->type->as.object, val, mem, (u8*)out + uni->payload_offset, uni->tag_key, err);
    }
  }
  return false;
}
