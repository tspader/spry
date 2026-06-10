#include "test.h"
#include "spry/gen.h"

UTEST_MAIN()

typedef struct {
  const c8* name;
  const c8* prefix;
  spry_err_t err;
  const c8* endpoint;
  const c8* arg;
  const c8* path;
  const c8* detail;
} gen_case_t;

static sp_str_t gen_case_path(sp_mem_t mem, const c8* name, const c8* file) {
  return sp_fmt(mem, "{}/{}/{}", sp_fmt_cstr(SPRY_GEN_CASES), sp_fmt_cstr(name), sp_fmt_cstr(file)).value;
}

static void gen_write_file(sp_str_t path, sp_str_t content) {
  sp_io_file_writer_t w;
  if (sp_io_file_writer_from_path(&w, path) != SP_OK) return;
  sp_io_write_str(&w.base, content, SP_NULLPTR);
  sp_io_file_writer_close(&w);
}

static void gen_check_file(s32* utest_result, sp_mem_t mem, const c8* name, const c8* file, sp_str_t actual, bool update) {
  sp_str_t path = gen_case_path(mem, name, file);

  if (update) {
    gen_write_file(path, actual);
    UTEST_PRINTF("updated {}\n", sp_fmt_str(path));
    return;
  }

  sp_str_t expected;
  if (sp_io_read_file(mem, path, &expected) != SP_OK) {
    UTEST_PRINTF("missing golden {}; rerun with SPRY_GEN_UPDATE=1\n", sp_fmt_str(path));
    EXPECT_TRUE_MSG(false, "missing golden");
    return;
  }

  if (!sp_str_equal(actual, expected)) {
    sp_str_t actual_path = sp_fmt(mem, "{}.actual", sp_fmt_str(path)).value;
    gen_write_file(actual_path, actual);
    UTEST_PRINTF("{} does not match; wrote {}\n", sp_fmt_str(path), sp_fmt_str(actual_path));
    EXPECT_TRUE_MSG(false, "golden mismatch");
  }
}

static void gen_check_err(s32* utest_result, sp_mem_t mem, gen_case_t c, const spry_gen_err_t* error) {
  if (c.err != error->code) {
    UTEST_PRINTF("expected '{}' got '{}': {}\n", sp_fmt_str(spry_err_name(c.err)), sp_fmt_str(spry_err_name(error->code)), sp_fmt_str(spry_gen_err_str(mem, error)));
    EXPECT_TRUE_MSG(false, "error code mismatch");
    return;
  }
  switch (error->code) {
    case SPRY_ERR_GEN_PREFIX_MISSING:
    case SPRY_ERR_GEN_WRITE:
      break;
    case SPRY_ERR_GEN_PREFIX_INVALID:
      EXPECT_TRUE_MSG(sp_str_equal_cstr(error->as.prefix.name, c.prefix), "wrong prefix in error");
      break;
    case SPRY_ERR_GEN_ENDPOINT_INVALID:
    case SPRY_ERR_GEN_ENDPOINT_DUP:
      EXPECT_TRUE_MSG(sp_str_equal_cstr(error->as.endpoint.name, c.endpoint), "wrong endpoint in error");
      break;
    case SPRY_ERR_GEN_ARG_INVALID:
    case SPRY_ERR_GEN_ARG_RESERVED:
    case SPRY_ERR_GEN_ARG_DUP:
    case SPRY_ERR_GEN_ARG_TYPE:
      EXPECT_TRUE_MSG(sp_str_equal_cstr(error->as.arg.endpoint, c.endpoint), "wrong endpoint in error");
      EXPECT_TRUE_MSG(sp_str_equal_cstr(error->as.arg.name, c.arg), "wrong argument in error");
      break;
    default:
      EXPECT_TRUE_MSG(sp_str_equal_cstr(error->as.parse.path, c.path), "wrong path in parse error");
      if (c.detail) EXPECT_TRUE_MSG(sp_str_equal_cstr(error->as.parse.detail, c.detail), "wrong detail in parse error");
      break;
  }
}

static void run_gen_case(s32* utest_result, gen_case_t c) {
  sp_mem_arena_t* arena = sp_mem_arena_new(sp_mem_os_new());
  sp_mem_t mem = sp_mem_arena_as_allocator(arena);
  bool update = !sp_str_empty(sp_os_env_get(sp_str_lit("SPRY_GEN_UPDATE")));

  sp_str_t input;
  ASSERT_EQ((s32)SP_OK, (s32)sp_io_read_file(mem, gen_case_path(mem, c.name, "endpoints.json"), &input));

  spry_gen_opts_t opts = { .prefix = sp_cstr_as_str(c.prefix ? c.prefix : "test"), .endpoints = input };

  sp_io_dyn_mem_writer_t out_c;
  sp_io_dyn_mem_writer_init(mem, &out_c);
  spry_gen_err_t err_c = spry_gen_c(mem, opts, &out_c.base);

  sp_io_dyn_mem_writer_t out_ts;
  sp_io_dyn_mem_writer_init(mem, &out_ts);
  spry_gen_err_t err_ts = spry_gen_ts(mem, opts, &out_ts.base);

  ASSERT_TRUE_MSG(err_c.code == err_ts.code, "c and ts generation disagree");

  if (c.err == SPRY_OK) {
    if (err_c.code != SPRY_OK) {
      UTEST_PRINTF("unexpected error: {}\n", sp_fmt_str(spry_gen_err_str(mem, &err_c)));
      ASSERT_TRUE_MSG(false, "generation failed");
    }
    if (update) sp_fs_create_dir(gen_case_path(mem, c.name, "golden"));
    gen_check_file(utest_result, mem, c.name, "golden/endpoints.h", sp_io_dyn_mem_writer_as_str(&out_c), update);
    gen_check_file(utest_result, mem, c.name, "golden/endpoints.ts", sp_io_dyn_mem_writer_as_str(&out_ts), update);
  } else {
    gen_check_err(utest_result, mem, c, &err_c);
    gen_check_err(utest_result, mem, c, &err_ts);
    if (update) {
      sp_fs_remove_file(gen_case_path(mem, c.name, "golden/endpoints.h"));
      sp_fs_remove_file(gen_case_path(mem, c.name, "golden/endpoints.ts"));
    } else {
      EXPECT_TRUE_MSG(!sp_fs_exists(gen_case_path(mem, c.name, "golden/endpoints.h")), "stale endpoints.h golden; rerun with SPRY_GEN_UPDATE=1");
      EXPECT_TRUE_MSG(!sp_fs_exists(gen_case_path(mem, c.name, "golden/endpoints.ts")), "stale endpoints.ts golden; rerun with SPRY_GEN_UPDATE=1");
    }
  }

  sp_mem_arena_destroy(arena);
}

UTEST_EMPTY_FIXTURE(gen)

UTEST_F(gen, demo) {
  run_gen_case(utest_result, (gen_case_t){ .name = "demo", .prefix = "demo" });
}

UTEST_F(gen, types) {
  run_gen_case(utest_result, (gen_case_t){ .name = "types", .prefix = "test" });
}

UTEST_F(gen, naming) {
  run_gen_case(utest_result, (gen_case_t){ .name = "naming", .prefix = "my_app2" });
}

UTEST_F(gen, empty) {
  run_gen_case(utest_result, (gen_case_t){ .name = "empty", .prefix = "test" });
}

UTEST_F(gen, bad_endpoint_name) {
  run_gen_case(utest_result, (gen_case_t){ .name = "bad-endpoint-name", .err = SPRY_ERR_GEN_ENDPOINT_INVALID, .endpoint = "get-users" });
}

UTEST_F(gen, bad_arg_name) {
  run_gen_case(utest_result, (gen_case_t){ .name = "bad-arg-name", .err = SPRY_ERR_GEN_ARG_INVALID, .endpoint = "scan", .arg = "a-b" });
}

UTEST_F(gen, reserved_arg) {
  run_gen_case(utest_result, (gen_case_t){ .name = "reserved-arg", .err = SPRY_ERR_GEN_ARG_RESERVED, .endpoint = "scan", .arg = "offsetof" });
}

UTEST_F(gen, dup_arg) {
  run_gen_case(utest_result, (gen_case_t){ .name = "dup-arg", .err = SPRY_ERR_GEN_ARG_DUP, .endpoint = "scan", .arg = "limit" });
}

UTEST_F(gen, dup_endpoint) {
  run_gen_case(utest_result, (gen_case_t){ .name = "dup-endpoint", .err = SPRY_ERR_GEN_ENDPOINT_DUP, .endpoint = "scan" });
}

UTEST_F(gen, bad_prefix) {
  run_gen_case(utest_result, (gen_case_t){ .name = "bad-prefix", .prefix = "my-app", .err = SPRY_ERR_GEN_PREFIX_INVALID });
}

UTEST_F(gen, bad_type) {
  run_gen_case(utest_result, (gen_case_t){ .name = "bad-type", .err = SPRY_ERR_AST_INVALID_ENUM, .path = "$.scan.args.properties.depth.type", .detail = "int64" });
}

UTEST_F(gen, bad_json) {
  run_gen_case(utest_result, (gen_case_t){ .name = "bad-json", .err = SPRY_ERR_JSON, .path = "$" });
}
