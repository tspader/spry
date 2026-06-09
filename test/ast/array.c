#include "ast/test.h"

#define ARRAY_MAX_TEXT 4

typedef struct {
  u32       count;
  const c8* texts[ARRAY_MAX_TEXT];
} array_expect_t;

static void compare_array(s32* utest_result, const void* actual_v, const void* expect_v) {
  spry_node_t* const* slot = (spry_node_t* const*)actual_v;
  spry_node_t* nodes = *slot;
  const array_expect_t* expect = (const array_expect_t*)expect_v;

  EXPECT_EQ((u64)expect->count, sp_da_size(nodes));
  sp_for(i, sp_min(expect->count, (u32)sp_da_size(nodes))) {
    EXPECT_EQ((s32)SPRY_NODE_KIND_TEXT, (s32)nodes[i].kind);
    ast_expect_str(utest_result, nodes[i].as.text.props.text, expect->texts[i]);
  }
}

typedef struct {
  const c8*      json;
  array_expect_t expect;
  ast_err_t      errors[AST_MAX_ERRORS];
} array_case_t;

static void run_array_case(s32* utest_result, array_case_t c) {
  run_ast_case(utest_result, (ast_case_t){
    .json     = c.json,
    .type     = &spry_node_array_type,
    .out_size = sizeof(spry_node_t*),
    .expect   = &c.expect,
    .compare  = compare_array,
    .errors   = c.errors,
  });
}

UTEST_EMPTY_FIXTURE(array)

UTEST_F(array, elements) {
  run_array_case(utest_result, (array_case_t){
    .json = "test/ast/json/array.ok.json",
    .expect = { .count = 2, .texts = { "a", "b" } },
  });
}

UTEST_F(array, empty) {
  run_array_case(utest_result, (array_case_t){
    .json = "test/ast/json/array.empty.json",
    .expect = { .count = 0 },
  });
}

UTEST_F(array, not_an_array) {
  run_array_case(utest_result, (array_case_t){
    .json = "test/ast/json/array.not_array.json",
    .errors = { { .code = SPRY_ERR_AST_EXPECTED_ARRAY, .path = "$" } },
  });
}

UTEST_F(array, element_error_carries_index) {
  run_array_case(utest_result, (array_case_t){
    .json = "test/ast/json/array.bad_elem.json",
    .errors = { { .code = SPRY_ERR_AST_UNKNOWN_VARIANT, .path = "$[1]", .detail = "blob" } },
  });
}

UTEST_F(array, nested_error_path) {
  run_array_case(utest_result, (array_case_t){
    .json = "test/ast/json/array.deep.json",
    .errors = { { .code = SPRY_ERR_AST_EXPECTED_INT, .path = "$[0].props.gap" } },
  });
}
