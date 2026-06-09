#ifndef SPRY_AST_TEST_H
#define SPRY_AST_TEST_H

#include "../test.h"
#include "abi/ui.gen.h"

#define AST_MAX_ERRORS 8

typedef struct {
  spry_err_t code;
  const c8*  path;
  const c8*  detail;
} ast_err_t;

typedef void (*ast_compare_fn)(s32* utest_result, const void* actual, const void* expect);

typedef struct {
  const c8*              json;
  const spry_ast_type_t* type;
  size_t                 out_size;
  const void*            expect;
  ast_compare_fn         compare;
  const ast_err_t*       errors;
} ast_case_t;

void run_ast_case(s32* utest_result, ast_case_t c);
u32  ast_count_errors(const ast_err_t* errors);
void ast_expect_str(s32* utest_result, sp_str_t actual, const c8* expected);

#endif
