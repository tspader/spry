#include "ast/test.h"

typedef struct {
  s32       event;
  const c8* handler;
  s32       onResponse;
  const c8* target;
} object_expect_t;

static void compare_object(s32* utest_result, const void* actual_v, const void* expect_v) {
  const spry_invoke_t*   actual = (const spry_invoke_t*)actual_v;
  const object_expect_t* expect = (const object_expect_t*)expect_v;
  EXPECT_EQ(expect->event, (s32)actual->event);
  ast_expect_str(utest_result, actual->handler, expect->handler);
  EXPECT_EQ(expect->onResponse, (s32)actual->onResponse);
  ast_expect_str(utest_result, actual->target, expect->target);
}

typedef struct {
  const c8*       json;
  object_expect_t expect;
  ast_err_t       errors[AST_MAX_ERRORS];
} object_case_t;

static void run_object_case(s32* utest_result, object_case_t c) {
  run_ast_case(utest_result, (ast_case_t){
    .json     = c.json,
    .type     = &spry_invoke_type,
    .out_size = sizeof(spry_invoke_t),
    .expect   = &c.expect,
    .compare  = compare_object,
    .errors   = c.errors,
  });
}

UTEST_EMPTY_FIXTURE(object)

UTEST_F(object, all_fields) {
  run_object_case(utest_result, (object_case_t){
    .json = "test/ast/json/object.ok.json",
    .expect = {
      .event = SPRY_EVENT_CLICK,
      .handler = "greet", .target = "root",
      .onResponse = SPRY_ONRESPONSE_PATCH },
  });
}

UTEST_F(object, not_an_object) {
  run_object_case(utest_result, (object_case_t){
    .json = "test/ast/json/object.not_object.json",
    .errors = { { .code = SPRY_ERR_AST_EXPECTED_OBJECT, .path = "$" } },
  });
}

UTEST_F(object, missing_required_accumulates) {
  run_object_case(utest_result, (object_case_t){
    .json = "test/ast/json/object.missing.json",
    .errors = {
      { .code = SPRY_ERR_AST_MISSING_KEY, .path = "$.handler", .detail = "handler" },
      { .code = SPRY_ERR_AST_MISSING_KEY, .path = "$.onResponse", .detail = "onResponse" },
    },
  });
}

UTEST_F(object, unknown_key) {
  run_object_case(utest_result, (object_case_t){
    .json = "test/ast/json/object.unknown.json",
    .errors = { { .code = SPRY_ERR_AST_UNKNOWN_KEY, .path = "$.bogus", .detail = "bogus" } },
  });
}

UTEST_F(object, field_wrong_type) {
  run_object_case(utest_result, (object_case_t){
    .json = "test/ast/json/object.wrong_type.json",
    .errors = { { .code = SPRY_ERR_AST_EXPECTED_STR, .path = "$.handler" } },
  });
}
