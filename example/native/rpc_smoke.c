#include "sp.h"
#include <sqlite3.h>
#include "spry/rpc.h"
#include "handlers.h"

static u32 g_failures;

static spry_reply_t run(demo_ctx_t* demo, const c8* name, const c8* body, deliver_outcome_t want) {
  spry_reply_t reply = spry_rpc_dispatch(demo->rpc, sp_cstr_as_str(name), sp_cstr_as_str(body));
  bool ok = reply.outcome == want;
  if (!ok) g_failures += 1;
  sp_log("{} {} {} -> outcome={} body={}", sp_fmt_cstr(ok ? "ok  " : "FAIL"), sp_fmt_cstr(name), sp_fmt_cstr(body), sp_fmt_uint(reply.outcome), sp_fmt_str(reply.body));
  return reply;
}

static void expect_contains(spry_reply_t reply, const c8* needle) {
  sp_str_t hay = reply.body;
  sp_str_t want = sp_cstr_as_str(needle);
  bool found = false;
  if (want.len <= hay.len) {
    sp_for(i, hay.len - want.len + 1) {
      sp_str_t window = sp_str(hay.data + i, want.len);
      if (sp_str_equal(window, want)) { found = true; break; }
    }
  }
  if (!found) {
    g_failures += 1;
    sp_log("FAIL body does not contain '{}'", sp_fmt_cstr(needle));
  }
}

s32 main(s32 argc, c8** argv) {
  if (argc < 2) {
    sp_log("usage: rpc-smoke <endpoints.json>");
    return 1;
  }

  sp_mem_t mem = sp_mem_os_new();

  sp_str_t endpoints_json;
  if (sp_io_read_file(mem, sp_cstr_as_str(argv[1]), &endpoints_json) != SP_OK) {
    sp_log("rpc-smoke: cannot read endpoints {}", sp_fmt_cstr(argv[1]));
    return 1;
  }
  spry_endpoints_t endpoints;
  spry_issue_t issue = sp_zero_s(spry_issue_t);
  if (spry_endpoints_parse(mem, endpoints_json, &endpoints, &issue)) {
    sp_log("rpc-smoke: invalid endpoints: {}", sp_fmt_str(spry_issue_str(mem, &issue)));
    return 1;
  }

  sqlite3* db;
  if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
    sp_log("rpc-smoke: cannot open :memory:");
    return 1;
  }
  if (!demo_db_seed(db)) {
    sp_log("rpc-smoke: seed failed: {}", sp_fmt_cstr(sqlite3_errmsg(db)));
    return 1;
  }
  demo_ctx_t* demo = demo_new(mem, db, endpoints);
  if (!spry_rpc_check(demo->rpc)) return 1;

  spry_reply_t tables = run(demo, "tables", "{}", DELIVER_OK);
  expect_contains(tables, "albums");
  expect_contains(tables, "artists");

  spry_reply_t grid = run(demo, "open_table", "{\"table\":\"albums\"}", DELIVER_OK);
  expect_contains(grid, "Tago Mago");
  expect_contains(grid, "edit_cell");

  run(demo, "open_table", "{\"table\":\"nope\"}", DELIVER_FAULT);
  spry_reply_t invalid = run(demo, "open_table", "{}", DELIVER_FAULT);
  expect_contains(invalid, "\"issues\"");
  expect_contains(invalid, "\"missing\"");
  spry_reply_t unknown = run(demo, "open_table", "{\"table\":\"albums\",\"bogus\":1}", DELIVER_FAULT);
  expect_contains(unknown, "\"unknown\"");
  run(demo, "bogus", "{}", DELIVER_FAULT);

  spry_reply_t editor = run(demo, "edit_cell", "{\"table\":\"albums\",\"rowid\":1,\"column\":\"title\"}", DELIVER_OK);
  expect_contains(editor, "\"input\"");
  expect_contains(editor, "save_cell");

  spry_reply_t saved = run(demo, "save_cell", "{\"table\":\"albums\",\"rowid\":1,\"column\":\"title\",\"value\":\"Tago Mago!\"}", DELIVER_OK);
  expect_contains(saved, "Tago Mago!");

  spry_reply_t counted = run(demo, "exec", "{\"sql\":\"select count(*) as n from albums\"}", DELIVER_OK);
  expect_contains(counted, "\"7\"");

  spry_reply_t broken = run(demo, "exec", "{\"sql\":\"selekt\"}", DELIVER_OK);
  expect_contains(broken, "SQL error");

  sqlite3_close(db);

  if (g_failures) {
    sp_log("rpc-smoke: {} failures", sp_fmt_uint(g_failures));
    return 1;
  }
  sp_log("rpc-smoke: all checks passed");
  return 0;
}
