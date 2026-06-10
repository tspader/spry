#include "ast/test.h"

typedef struct {
  sp_str_t key;
  sp_str_t value;
} values_entry_t;

typedef struct {
  values_entry_t* entries;
} values_out_t;

static const spry_ast_type_t values_str_type = {
  .kind = SPRY_AST_VALUES,
  .as.values = {
    .value = &spry_str_type,
    .stride = sizeof(values_entry_t),
    .key_offset = offsetof(values_entry_t, key),
    .value_offset = offsetof(values_entry_t, value),
  },
};

typedef struct {
  const c8*      json;
  ast_compare_fn compare;
  ast_err_t      errors[AST_MAX_ERRORS];
} values_case_t;

static void run_values_case(s32* utest_result, values_case_t c) {
  run_ast_case(utest_result, (ast_case_t){
    .json     = c.json,
    .type     = &values_str_type,
    .out_size = sizeof(values_out_t),
    .compare  = c.compare,
    .errors   = c.errors,
  });
}

static void compare_values(s32* utest_result, const void* actual_v, const void* expect_v) {
  (void)expect_v;
  const values_out_t* out = (const values_out_t*)actual_v;
  EXPECT_EQ((u64)2, sp_da_size(out->entries));
  if (sp_da_size(out->entries) == 2) {
    ast_expect_str(utest_result, out->entries[0].key, "table");
    ast_expect_str(utest_result, out->entries[0].value, "albums");
    ast_expect_str(utest_result, out->entries[1].key, "rowid");
    ast_expect_str(utest_result, out->entries[1].value, "3");
  }
}

UTEST_EMPTY_FIXTURE(values)

UTEST_F(values, ok) {
  run_values_case(utest_result, (values_case_t){
    .json    = "test/ast/json/values.ok.json",
    .compare = compare_values,
  });
}

UTEST_F(values, not_an_object) {
  run_values_case(utest_result, (values_case_t){
    .json   = "test/ast/json/values.not_object.json",
    .errors = { { .code = SPRY_ERR_AST_EXPECTED_OBJECT, .path = "$" } },
  });
}

UTEST_F(values, bad_element) {
  run_values_case(utest_result, (values_case_t){
    .json   = "test/ast/json/values.bad_elem.json",
    .errors = { { .code = SPRY_ERR_AST_EXPECTED_STR, .path = "$.rowid" } },
  });
}
