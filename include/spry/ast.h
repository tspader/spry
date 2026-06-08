#ifndef SPRY_AST_H
#define SPRY_AST_H

#include "sp.h"
#include "yyjson.h"

typedef enum {
  SPRY_AST_BOOL,
  SPRY_AST_I32,
  SPRY_AST_STR,
  SPRY_AST_ENUM,
  SPRY_AST_OBJECT,
  SPRY_AST_ARRAY,
  SPRY_AST_UNION,
} spry_ast_kind_t;

typedef struct spry_ast_type spry_ast_type_t;

typedef struct {
  const c8* key;
  u32 offset;
  const spry_ast_type_t* type;
  bool required;
  bool is_ptr;
  u32 size;
} spry_ast_field_t;

typedef struct {
  const c8* tag;
  s32 value;
  const spry_ast_type_t* type;
} spry_ast_variant_t;

typedef struct {
  const spry_ast_field_t* fields;
  u32 count;
} spry_ast_object_t;

typedef struct {
  const spry_ast_type_t* element;
  u32 stride;
} spry_ast_array_t;

typedef struct {
  const c8* tag_key;
  u32 tag_offset;
  u32 payload_offset;
  const spry_ast_variant_t* variants;
  u32 count;
} spry_ast_union_t;

typedef struct {
  const c8* const* names;
  u32 count;
} spry_ast_enum_t;

struct spry_ast_type {
  spry_ast_kind_t kind;
  union {
    spry_ast_object_t object;
    spry_ast_array_t  array;
    spry_ast_union_t  uni;
    spry_ast_enum_t   enom;
  } as;
};

bool spry_ast_parse(const spry_ast_type_t* type, yyjson_val* val, sp_mem_t mem, void* out, sp_str_t* err);

#endif
