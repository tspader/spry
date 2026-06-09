#include "ast/test.h"

typedef struct {
  const c8* json;
  ast_err_t errors[AST_MAX_ERRORS];
} union_case_t;

static void run_union_case(s32* utest_result, union_case_t c) {
  run_ast_case(utest_result, (ast_case_t){
    .json     = c.json,
    .type     = &spry_node_type,
    .out_size = sizeof(spry_node_t),
    .errors   = c.errors,
  });
}

UTEST_EMPTY_FIXTURE(node_union)

UTEST_F(node_union, not_an_object) {
  run_union_case(utest_result, (union_case_t){
    .json = "test/ast/json/union.not_object.json",
    .errors = { { .code = SPRY_ERR_AST_EXPECTED_OBJECT, .path = "$" } },
  });
}

UTEST_F(node_union, missing_discriminator) {
  run_union_case(utest_result, (union_case_t){
    .json = "test/ast/json/union.no_kind.json",
    .errors = { { .code = SPRY_ERR_AST_MISSING_DISCRIMINATOR, .path = "$", .detail = "kind" } },
  });
}

UTEST_F(node_union, non_string_discriminator) {
  run_union_case(utest_result, (union_case_t){
    .json = "test/ast/json/union.kind_type.json",
    .errors = { { .code = SPRY_ERR_AST_MISSING_DISCRIMINATOR, .path = "$", .detail = "kind" } },
  });
}

UTEST_F(node_union, unknown_variant) {
  run_union_case(utest_result, (union_case_t){
    .json = "test/ast/json/union.bad_kind.json",
    .errors = { { .code = SPRY_ERR_AST_UNKNOWN_VARIANT, .path = "$", .detail = "blob" } },
  });
}

UTEST_F(node_union, discriminator_not_flagged_unknown) {
  run_union_case(utest_result, (union_case_t){
    .json = "test/ast/json/union.extra_key.json",
    .errors = { { .code = SPRY_ERR_AST_UNKNOWN_KEY, .path = "$.extra", .detail = "extra" } },
  });
}

UTEST_F(node_union, unknown_nested_key) {
  run_union_case(utest_result, (union_case_t){
    .json = "test/ast/json/union.nested_key.json",
    .errors = { { .code = SPRY_ERR_AST_UNKNOWN_KEY, .path = "$.props.bogus", .detail = "bogus" } },
  });
}

UTEST_F(node_union, missing_nested_required) {
  run_union_case(utest_result, (union_case_t){
    .json = "test/ast/json/union.no_props.json",
    .errors = { { .code = SPRY_ERR_AST_MISSING_KEY, .path = "$.props", .detail = "props" } },
  });
}

UTEST_F(node_union, errors_accumulate_across_levels) {
  run_union_case(utest_result, (union_case_t){
    .json = "test/ast/json/union.multi.json",
    .errors = {
      { .code = SPRY_ERR_AST_EXPECTED_STR, .path = "$.props.text" },
      { .code = SPRY_ERR_AST_UNKNOWN_KEY,  .path = "$.extra", .detail = "extra" },
    },
  });
}

static void compare_tree(s32* utest_result, const void* actual_v, const void* expect_v) {
  (void)expect_v;
  const spry_node_t* node = (const spry_node_t*)actual_v;

  EXPECT_EQ((s32)SPRY_NODE_KIND_BOX, (s32)node->kind);
  const spry_box_t* box = &node->as.box;
  ast_expect_str(utest_result, box->id, "root");

  EXPECT_TRUE(box->props != SP_NULLPTR);
  if (box->props) {
    EXPECT_EQ((s32)SPRY_DIRECTION_COLUMN, (s32)box->props->direction);
    EXPECT_EQ(8, box->props->gap);
    EXPECT_EQ((s32)SPRY_JUSTIFY_BETWEEN, (s32)box->props->justify);
  }

  EXPECT_EQ((u64)2, sp_da_size(box->children));
  if (sp_da_size(box->children) == 2) {
    EXPECT_EQ((s32)SPRY_NODE_KIND_TEXT, (s32)box->children[0].kind);
    ast_expect_str(utest_result, box->children[0].as.text.props.text, "hi");

    EXPECT_EQ((s32)SPRY_NODE_KIND_BUTTON, (s32)box->children[1].kind);
    const spry_button_t* btn = &box->children[1].as.button;
    EXPECT_TRUE(btn->on != SP_NULLPTR);
    if (btn->on) {
      EXPECT_EQ((s32)SPRY_EVENT_CLICK, (s32)btn->on->event);
      EXPECT_EQ((s32)SPRY_SWAP_INNER, (s32)btn->on->swap);
      ast_expect_str(utest_result, btn->on->action, "/a");
    }
  }
}

UTEST_F(node_union, valid_tree) {
  run_ast_case(utest_result, (ast_case_t){
    .json     = "test/ast/json/union.tree.json",
    .type     = &spry_node_type,
    .out_size = sizeof(spry_node_t),
    .compare  = compare_tree,
  });
}
