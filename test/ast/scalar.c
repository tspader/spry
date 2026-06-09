#include "ast/test.h"

typedef struct {
  s32 direction;
  s32 gap;
  s32 padding;
  s32 align;
  s32 justify;
} scalar_expect_t;

static void compare_scalar(s32* utest_result, const void* actual_v, const void* expect_v) {
  const spry_box_props_t* actual = (const spry_box_props_t*)actual_v;
  const scalar_expect_t*  expect = (const scalar_expect_t*)expect_v;
  EXPECT_EQ(expect->direction, (s32)actual->direction);
  EXPECT_EQ(expect->gap,       actual->gap);
  EXPECT_EQ(expect->padding,   actual->padding);
  EXPECT_EQ(expect->align,     (s32)actual->align);
  EXPECT_EQ(expect->justify,   (s32)actual->justify);
}

typedef struct {
  const c8*       json;
  scalar_expect_t expect;
  ast_err_t       errors[AST_MAX_ERRORS];
} scalar_case_t;

static void run_scalar_case(s32* utest_result, scalar_case_t c) {
  run_ast_case(utest_result, (ast_case_t){
    .json     = c.json,
    .type     = &spry_box_props_type,
    .out_size = sizeof(spry_box_props_t),
    .expect   = &c.expect,
    .compare  = compare_scalar,
    .errors   = c.errors,
  });
}

UTEST_EMPTY_FIXTURE(scalar)

UTEST_F(scalar, all_fields) {
  run_scalar_case(utest_result, (scalar_case_t){
    .json = "test/ast/json/scalar.ok.json",
    .expect = {
      .direction = SPRY_DIRECTION_COLUMN,
      .gap = 8, .padding = 16,
      .align = SPRY_ALIGN_STRETCH,
      .justify = SPRY_JUSTIFY_BETWEEN },
  });
}

UTEST_F(scalar, optional_absent_zeroed) {
  run_scalar_case(utest_result, (scalar_case_t){
    .json = "test/ast/json/scalar.empty.json",
  });
}

UTEST_F(scalar, int_wrong_type) {
  run_scalar_case(utest_result, (scalar_case_t){
    .json = "test/ast/json/scalar.bad_int.json",
    .errors = { { .code = SPRY_ERR_AST_EXPECTED_NUMBER, .path = "$.gap" } },
  });
}

UTEST_F(scalar, int_out_of_range) {
  run_scalar_case(utest_result, (scalar_case_t){
    .json = "test/ast/json/scalar.int_range.json",
    .errors = { { .code = SPRY_ERR_AST_INT_RANGE, .path = "$.gap" } },
  });
}

UTEST_F(scalar, enum_invalid_value) {
  run_scalar_case(utest_result, (scalar_case_t){
    .json = "test/ast/json/scalar.bad_enum.json",
    .errors = { { .code = SPRY_ERR_AST_INVALID_ENUM, .path = "$.align", .detail = "middle" } },
  });
}

UTEST_F(scalar, enum_wrong_type) {
  run_scalar_case(utest_result, (scalar_case_t){
    .json = "test/ast/json/scalar.enum_type.json",
    .errors = { { .code = SPRY_ERR_AST_EXPECTED_ENUM, .path = "$.direction" } },
  });
}
