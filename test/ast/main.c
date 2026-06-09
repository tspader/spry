#include "ast/scalar.c"
#include "ast/object.c"
#include "ast/array.c"
#include "ast/union.c"

#define SP_IMPLEMENTATION
#include "sp.h"

UTEST_MAIN()

u32 ast_count_errors(const ast_err_t* errors) {
  u32 n = 0;
  if (!errors) return 0;
  while (n < AST_MAX_ERRORS && errors[n].code != SPRY_OK) n++;
  return n;
}

void ast_expect_str(s32* utest_result, sp_str_t actual, const c8* expected) {
  if (!expected) {
    EXPECT_TRUE(sp_str_empty(actual));
    return;
  }
  EXPECT_TRUE(sp_str_equal_cstr(actual, expected));
}

void run_ast_case(s32* utest_result, ast_case_t c) {
  sp_mem_arena_t* arena = sp_mem_arena_new(sp_mem_os_new());
  sp_mem_t mem = sp_mem_arena_as_allocator(arena);

  sp_str_t content;
  ASSERT_EQ((s32)SP_OK, (s32)sp_io_read_file(mem, sp_cstr_as_str(c.json), &content));

  yyjson_doc* doc = yyjson_read(content.data, content.len, 0);
  ASSERT_TRUE(doc != SP_NULLPTR);
  yyjson_val* root = yyjson_doc_get_root(doc);

  void* actual = sp_alloc(mem, c.out_size);
  sp_mem_zero(actual, c.out_size);

  spry_ctx_t ctx;
  spry_ctx_init(&ctx, mem);
  spry_ast_parse(c.type, root, &ctx, actual);

  u32 expected = ast_count_errors(c.errors);
  EXPECT_EQ((u64)expected, sp_da_size(ctx.issues));

  sp_for(i, sp_min(expected, (u32)sp_da_size(ctx.issues))) {
    const ast_err_t*    want = &c.errors[i];
    const spry_issue_t* got  = &ctx.issues[i];
    EXPECT_EQ((s32)want->code, (s32)got->code);
    if (want->path)   EXPECT_TRUE(sp_str_equal_cstr(got->path, want->path));
    if (want->detail) EXPECT_TRUE(sp_str_equal_cstr(got->detail, want->detail));
  }

  if (expected == 0 && c.compare) c.compare(utest_result, actual, c.expect);

  yyjson_doc_free(doc);
  sp_mem_arena_destroy(arena);
}
