#include "test.h"
#include "spry/ui.h"
#include "spry/core.h"

UTEST_MAIN()

static sp_str_t ui_build_conformance(sp_mem_t mem) {
  spry_ui_t* ui = spry_ui_new(mem);
  SPRY_UI(ui) {
    SPRY_COLUMN(.gap = 4, .id = sp_str_lit("root")) {
      SPRY_TEXT("spry");
      SPRY_LINK("docs", .href = sp_str_lit("https://example.com"));
      SPRY_ROW(.gap = 16, .align = SPRY_ALIGN_CENTER) {
        SPRY_INPUT("q", .placeholder = sp_str_lit("search"));
        SPRY_BUTTON("Go", .on = SPRY_INVOKE(.event = SPRY_EVENT_SUBMIT, .handler = sp_str_lit("search"), .onResponse = SPRY_ONRESPONSE_PATCH, .target = sp_str_lit("results"))) {
          SPRY_ARG("q", "x");
          SPRY_ARG("page", 1);
        }
      }
      SPRY_BUTTON("Refresh", .on = SPRY_PATCH("refresh", "root")) {
        SPRY_ARG("count", 3);
      }
      SPRY_BOX();
      SPRY_TEXTF("ready {} go", sp_fmt_int(2));
      SPRY_TEXT("status text", .id = sp_str_lit("status"));
    }
  }
  return spry_ui_write(ui, spry_ui_root(ui));
}

static void ui_canon(s32* utest_result, sp_mem_t mem, sp_str_t json, sp_str_t* out) {
  spry_node_t node = sp_zero_s(spry_node_t);
  sp_str_t error = sp_zero_s(sp_str_t);
  if (!spry_node_parse(mem, json, &node, &error)) {
    UTEST_PRINTF("parse failed: {}\n", sp_fmt_str(error));
    ASSERT_TRUE_MSG(false, "fragment did not parse");
  }
  *out = spry_ast_write(mem, &spry_node_type, &node);
}

UTEST_EMPTY_FIXTURE(ui)

UTEST_F(ui, conformance) {
  sp_mem_arena_t* arena = sp_mem_arena_new(sp_mem_os_new());
  sp_mem_t mem = sp_mem_arena_as_allocator(arena);

  sp_str_t c_json = ui_build_conformance(mem);

  sp_str_t jsx_json;
  ASSERT_EQ_MSG((s32)SP_OK, (s32)sp_io_read_file(mem, sp_str_lit(SPRY_UI_CONFORMANCE), &jsx_json), "missing jsx conformance output; rebuild");

  sp_str_t c_canon;
  ui_canon(utest_result, mem, c_json, &c_canon);
  sp_str_t jsx_canon;
  ui_canon(utest_result, mem, jsx_json, &jsx_canon);

  EXPECT_TRUE_MSG(sp_str_equal(c_json, c_canon), "c writer output is not canonical");
  if (!sp_str_equal(c_canon, jsx_canon)) {
    UTEST_PRINTF("c:   {}\njsx: {}\n", sp_fmt_str(c_canon), sp_fmt_str(jsx_canon));
    EXPECT_TRUE_MSG(false, "c and jsx trees differ");
  }

  sp_mem_arena_destroy(arena);
}
