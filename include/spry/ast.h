#ifndef SPRY_AST_H
#define SPRY_AST_H

#include "sp.h"
#include "yyjson.h"

typedef enum {
  SPRY_AST_BOOL,
  SPRY_AST_NUMBER,
  SPRY_AST_STR,
  SPRY_AST_ENUM,
  SPRY_AST_OBJECT,
  SPRY_AST_ARRAY,
  SPRY_AST_UNION,
  SPRY_AST_VALUES,
} spry_ast_kind_t;

typedef enum {
  SPRY_NUM_S8,
  SPRY_NUM_U8,
  SPRY_NUM_S16,
  SPRY_NUM_U16,
  SPRY_NUM_S32,
  SPRY_NUM_U32,
  SPRY_NUM_F32,
  SPRY_NUM_F64,
} spry_num_repr_t;

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
  const spry_ast_type_t* value;
  u32 stride;
  u32 key_offset;
  u32 value_offset;
} spry_ast_values_t;

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

typedef struct {
  spry_num_repr_t repr;
} spry_ast_number_t;

struct spry_ast_type {
  spry_ast_kind_t kind;
  union {
    spry_ast_object_t object;
    spry_ast_array_t  array;
    spry_ast_values_t values;
    spry_ast_union_t  uni;
    spry_ast_enum_t   enom;
    spry_ast_number_t number;
  } as;
};

typedef enum {
  SPRY_OK = 0,

  SPRY_ERR_AST = 1000,
  SPRY_ERR_AST_EXPECTED_BOOL,
  SPRY_ERR_AST_EXPECTED_NUMBER,
  SPRY_ERR_AST_EXPECTED_STR,
  SPRY_ERR_AST_EXPECTED_ENUM,
  SPRY_ERR_AST_EXPECTED_ARRAY,
  SPRY_ERR_AST_EXPECTED_OBJECT,
  SPRY_ERR_AST_INT_RANGE,
  SPRY_ERR_AST_INT_FRACTIONAL,
  SPRY_ERR_AST_INVALID_ENUM,
  SPRY_ERR_AST_UNKNOWN_KEY,
  SPRY_ERR_AST_MISSING_KEY,
  SPRY_ERR_AST_MISSING_DISCRIMINATOR,
  SPRY_ERR_AST_UNKNOWN_VARIANT,

  SPRY_ERR_JSON = 2000,

  SPRY_ERR_GEN_PREFIX_MISSING = 3000,
  SPRY_ERR_GEN_PREFIX_INVALID,
  SPRY_ERR_GEN_ENDPOINT_INVALID,
  SPRY_ERR_GEN_ENDPOINT_DUP,
  SPRY_ERR_GEN_ARG_INVALID,
  SPRY_ERR_GEN_ARG_RESERVED,
  SPRY_ERR_GEN_ARG_DUP,
  SPRY_ERR_GEN_ARG_TYPE,
  SPRY_ERR_GEN_WRITE,
} spry_err_t;

typedef struct {
  spry_err_t code;
  sp_str_t   path;
  sp_str_t   detail;
  f64        num;
  s64        min;
  s64        max;
} spry_issue_t;

#define SPRY_PATH_MAX 32

typedef enum {
  SPRY_PATH_KEY,
  SPRY_PATH_INDEX,
} spry_path_seg_kind_t;

typedef struct {
  spry_path_seg_kind_t kind;
  const c8* key;
  u32 index;
} spry_path_seg_t;

typedef struct {
  sp_mem_t mem;
  spry_path_seg_t path[SPRY_PATH_MAX];
  u32 depth;
  sp_da(spry_issue_t) issues;
} spry_ctx_t;

extern const spry_ast_type_t spry_str_type;
extern const spry_ast_type_t spry_bool_type;
extern const spry_ast_type_t spry_s8_type;
extern const spry_ast_type_t spry_u8_type;
extern const spry_ast_type_t spry_s16_type;
extern const spry_ast_type_t spry_u16_type;
extern const spry_ast_type_t spry_s32_type;
extern const spry_ast_type_t spry_u32_type;
extern const spry_ast_type_t spry_f32_type;
extern const spry_ast_type_t spry_f64_type;

void spry_ctx_init(spry_ctx_t* ctx, sp_mem_t mem);

spry_err_t spry_ast_parse(const spry_ast_type_t* type, yyjson_val* val, spry_ctx_t* ctx, void* out);

void spry_json_push(sp_da(c8)* out, sp_str_t raw);
void spry_json_push_str(sp_da(c8)* out, sp_str_t value);
sp_str_t spry_ast_write(sp_mem_t mem, const spry_ast_type_t* type, const void* value);

sp_str_t spry_err_name(spry_err_t code);
sp_str_t spry_issue_str(sp_mem_t mem, const spry_issue_t* issue);

#endif
