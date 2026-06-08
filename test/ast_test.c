#define SP_IMPLEMENTATION
#include "sp.h"
#include "abi/ui.gen.h"

static u32 g_fail = 0;

static void check(const c8* label, bool cond) {
  if (cond) {
    sp_log("  ok   {}", sp_fmt_cstr(label));
  } else {
    sp_log("  FAIL {}", sp_fmt_cstr(label));
    g_fail += 1;
  }
}

static bool parse(sp_mem_t mem, sp_str_t json, spry_node_t* out, sp_str_t* err) {
  yyjson_doc* doc = yyjson_read(json.data, json.len, 0);
  if (!doc) {
    *err = sp_str_lit("json read failed");
    return false;
  }
  yyjson_val* root = yyjson_doc_get_root(doc);
  bool ok = spry_ast_parse(&SPRY_AST_ROOT_TYPE, root, mem, out, err);
  yyjson_doc_free(doc);
  return ok;
}

static void expect_reject(sp_mem_t mem, const c8* label, sp_str_t json) {
  spry_node_t node = sp_zero_s(spry_node_t);
  sp_str_t err = sp_zero_s(sp_str_t);
  bool ok = parse(mem, json, &node, &err);
  if (ok) {
    sp_log("  FAIL {} (accepted, should reject)", sp_fmt_cstr(label));
    g_fail += 1;
  } else {
    sp_log("  ok   {} -> rejected: {}", sp_fmt_cstr(label), sp_fmt_str(err));
  }
}

s32 main(void) {
  sp_mem_t mem = sp_mem_os_new();

  sp_str_t tree = sp_str_lit(
    "{"
    "  \"kind\": \"box\","
    "  \"id\": \"root\","
    "  \"props\": { \"direction\": \"column\", \"gap\": 8, \"padding\": 16, \"align\": \"stretch\", \"justify\": \"between\" },"
    "  \"children\": ["
    "    { \"kind\": \"text\", \"props\": { \"text\": \"hi\" } },"
    "    { \"kind\": \"link\", \"props\": { \"text\": \"x\", \"href\": \"http://e\" } },"
    "    { \"kind\": \"input\", \"props\": { \"name\": \"n\", \"placeholder\": \"p\" } },"
    "    { \"kind\": \"button\", \"props\": { \"text\": \"go\" }, \"on\": { \"event\": \"click\", \"action\": \"/a\", \"target\": \"root\", \"swap\": \"inner\" } }"
    "  ]"
    "}");

  sp_log("valid tree:");
  spry_node_t node = sp_zero_s(spry_node_t);
  sp_str_t err = sp_zero_s(sp_str_t);
  bool ok = parse(mem, tree, &node, &err);
  check("parse accepted", ok);
  if (!ok) sp_log("  err: {}", sp_fmt_str(err));

  check("root is box", node.kind == SPRY_NODE_KIND_BOX);
  spry_box_t* box = &node.as.box;
  check("box.id == root", sp_str_equal_cstr(box->id, "root"));
  check("box.props present", box->props != SP_NULLPTR);
  check("props.direction == column", box->props && box->props->direction == SPRY_DIRECTION_COLUMN);
  check("props.gap == 8", box->props && box->props->gap == 8);
  check("props.padding == 16", box->props && box->props->padding == 16);
  check("props.align == stretch", box->props && box->props->align == SPRY_ALIGN_STRETCH);
  check("props.justify == between", box->props && box->props->justify == SPRY_JUSTIFY_BETWEEN);
  check("children count == 4", sp_da_size(box->children) == 4);

  if (sp_da_size(box->children) == 4) {
    spry_node_t* c0 = &box->children[0];
    check("child0 is text", c0->kind == SPRY_NODE_KIND_TEXT);
    check("child0.text == hi", sp_str_equal_cstr(c0->as.text.props.text, "hi"));

    spry_node_t* c1 = &box->children[1];
    check("child1 is link", c1->kind == SPRY_NODE_KIND_LINK);
    check("child1.href == http://e", sp_str_equal_cstr(c1->as.link.props.href, "http://e"));

    spry_node_t* c2 = &box->children[2];
    check("child2 is input", c2->kind == SPRY_NODE_KIND_INPUT);
    check("child2.name == n", sp_str_equal_cstr(c2->as.input.props.name, "n"));
    check("child2.placeholder == p", sp_str_equal_cstr(c2->as.input.props.placeholder, "p"));

    spry_node_t* c3 = &box->children[3];
    check("child3 is button", c3->kind == SPRY_NODE_KIND_BUTTON);
    check("child3.on present", c3->as.button.on != SP_NULLPTR);
    check("child3.on.event == click", c3->as.button.on && c3->as.button.on->event == SPRY_EVENT_CLICK);
    check("child3.on.swap == inner", c3->as.button.on && c3->as.button.on->swap == SPRY_SWAP_INNER);
    check("child3.on.action == /a", c3->as.button.on && sp_str_equal_cstr(c3->as.button.on->action, "/a"));
  }

  sp_log("rejections:");
  expect_reject(mem, "unknown variant", sp_str_lit("{\"kind\":\"blob\"}"));
  expect_reject(mem, "missing required props", sp_str_lit("{\"kind\":\"text\"}"));
  expect_reject(mem, "unknown top-level key", sp_str_lit("{\"kind\":\"text\",\"props\":{\"text\":\"x\"},\"extra\":1}"));
  expect_reject(mem, "wrong scalar type", sp_str_lit("{\"kind\":\"box\",\"props\":{\"gap\":\"big\"}}"));
  expect_reject(mem, "invalid enum value", sp_str_lit("{\"kind\":\"box\",\"props\":{\"align\":\"middle\"}}"));
  expect_reject(mem, "unknown nested key", sp_str_lit("{\"kind\":\"text\",\"props\":{\"text\":\"x\",\"bogus\":2}}"));
  expect_reject(mem, "missing discriminator", sp_str_lit("{\"props\":{\"text\":\"x\"}}"));

  if (g_fail == 0) {
    sp_log("ALL PASS");
    return 0;
  }
  sp_log("{} FAILURES", sp_fmt_uint(g_fail));
  return 1;
}
