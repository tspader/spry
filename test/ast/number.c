#include "ast/test.h"

typedef struct {
  s8  i8;
  u8  u8v;
  s16 i16;
  u16 u16v;
  s32 i32;
  u32 u32v;
  f32 f32v;
  f64 f64v;
} nums_t;

static const spry_ast_type_t nt_s8  = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_S8  } };
static const spry_ast_type_t nt_u8  = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_U8  } };
static const spry_ast_type_t nt_s16 = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_S16 } };
static const spry_ast_type_t nt_u16 = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_U16 } };
static const spry_ast_type_t nt_s32 = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_S32 } };
static const spry_ast_type_t nt_u32 = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_U32 } };
static const spry_ast_type_t nt_f32 = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_F32 } };
static const spry_ast_type_t nt_f64 = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = SPRY_NUM_F64 } };

static const spry_ast_field_t nums_fields[] = {
  { .key = "i8",  .offset = offsetof(nums_t, i8),   .type = &nt_s8  },
  { .key = "u8",  .offset = offsetof(nums_t, u8v),  .type = &nt_u8  },
  { .key = "i16", .offset = offsetof(nums_t, i16),  .type = &nt_s16 },
  { .key = "u16", .offset = offsetof(nums_t, u16v), .type = &nt_u16 },
  { .key = "i32", .offset = offsetof(nums_t, i32),  .type = &nt_s32 },
  { .key = "u32", .offset = offsetof(nums_t, u32v), .type = &nt_u32 },
  { .key = "f32", .offset = offsetof(nums_t, f32v), .type = &nt_f32 },
  { .key = "f64", .offset = offsetof(nums_t, f64v), .type = &nt_f64 },
};
static const spry_ast_type_t nums_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = nums_fields, .count = 8 } };

typedef struct {
  s8  i8;
  u8  u8v;
  s16 i16;
  u16 u16v;
  s32 i32;
  u32 u32v;
  f32 f32v;
  f64 f64v;
} nums_expect_t;

static void compare_nums(s32* utest_result, const void* actual_v, const void* expect_v) {
  const nums_t*        actual = (const nums_t*)actual_v;
  const nums_expect_t* expect = (const nums_expect_t*)expect_v;
  EXPECT_EQ((s32)expect->i8,   (s32)actual->i8);
  EXPECT_EQ((u32)expect->u8v,  (u32)actual->u8v);
  EXPECT_EQ((s32)expect->i16,  (s32)actual->i16);
  EXPECT_EQ((u32)expect->u16v, (u32)actual->u16v);
  EXPECT_EQ(expect->i32,       actual->i32);
  EXPECT_EQ(expect->u32v,      actual->u32v);
  EXPECT_EQ(expect->f32v,      actual->f32v);
  EXPECT_EQ(expect->f64v,      actual->f64v);
}

typedef struct {
  const c8*     json;
  nums_expect_t expect;
  ast_err_t     errors[AST_MAX_ERRORS];
} nums_case_t;

static void run_nums_case(s32* utest_result, nums_case_t c) {
  run_ast_case(utest_result, (ast_case_t){
    .json     = c.json,
    .type     = &nums_type,
    .out_size = sizeof(nums_t),
    .expect   = &c.expect,
    .compare  = compare_nums,
    .errors   = c.errors,
  });
}

UTEST_EMPTY_FIXTURE(number)

UTEST_F(number, all_widths) {
  run_nums_case(utest_result, (nums_case_t){
    .json = "test/ast/json/number.ok.json",
    .expect = {
      .i8 = -128, .u8v = 255, .i16 = -32768, .u16v = 65535,
      .i32 = 10, .u32v = 4294967295u, .f32v = 1.5f, .f64v = 2.5 },
  });
}

UTEST_F(number, fractional_rejected) {
  run_nums_case(utest_result, (nums_case_t){
    .json = "test/ast/json/number.fractional.json",
    .errors = { { .code = SPRY_ERR_AST_INT_FRACTIONAL, .path = "$.i32" } },
  });
}

UTEST_F(number, above_max) {
  run_nums_case(utest_result, (nums_case_t){
    .json = "test/ast/json/number.over.json",
    .errors = { { .code = SPRY_ERR_AST_INT_RANGE, .path = "$.u8" } },
  });
}

UTEST_F(number, below_min) {
  run_nums_case(utest_result, (nums_case_t){
    .json = "test/ast/json/number.under.json",
    .errors = { { .code = SPRY_ERR_AST_INT_RANGE, .path = "$.i8" } },
  });
}

UTEST_F(number, not_a_number) {
  run_nums_case(utest_result, (nums_case_t){
    .json = "test/ast/json/number.not_num.json",
    .errors = { { .code = SPRY_ERR_AST_EXPECTED_NUMBER, .path = "$.i32" } },
  });
}
