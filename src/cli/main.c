#include "sp.h"
#include "spry/gen.h"

static s32 cli_usage(void) {
  sp_log("usage: spry gen <endpoints.json> --prefix <name> [--c <out.h>] [--ts <out.ts>]");
  return 1;
}

static s32 cli_emit(sp_mem_t mem, spry_gen_opts_t opts, sp_str_t path, spry_gen_err_t (*gen)(sp_mem_t, spry_gen_opts_t, sp_io_writer_t*)) {
  sp_str_t tmp = sp_fmt(mem, "{}.tmp", sp_fmt_str(path)).value;

  sp_io_file_writer_t writer;
  if (sp_io_file_writer_from_path(&writer, tmp) != SP_OK) {
    sp_log("spry: cannot write {}", sp_fmt_str(tmp));
    return 1;
  }

  spry_gen_err_t error = gen(mem, opts, &writer.base);
  sp_err_t closed = sp_io_file_writer_close(&writer);
  if (!error.code && closed) error = (spry_gen_err_t){ .code = SPRY_ERR_GEN_WRITE, .as.write = closed };

  if (error.code) {
    sp_fs_remove_file(tmp);
    sp_log("spry: {}", sp_fmt_str(spry_gen_err_str(mem, &error)));
    return 1;
  }

  if (sp_sys_rename_s(SP_AT_FDCWD, tmp, SP_AT_FDCWD, path) != 0) {
    sp_fs_remove_file(tmp);
    sp_log("spry: cannot write {}", sp_fmt_str(path));
    return 1;
  }

  sp_log("spry: wrote {}", sp_fmt_str(path));
  return 0;
}

s32 main(s32 argc, c8** argv) {
  if (argc < 3 || !sp_cstr_equal(argv[1], "gen")) return cli_usage();

  sp_mem_t mem = sp_mem_os_new();
  sp_str_t input = sp_cstr_as_str(argv[2]);
  sp_str_t prefix = sp_zero_s(sp_str_t);
  sp_str_t c_out = sp_zero_s(sp_str_t);
  sp_str_t ts_out = sp_zero_s(sp_str_t);

  for (s32 i = 3; i < argc; i += 2) {
    if (i + 1 >= argc) return cli_usage();
    if (sp_cstr_equal(argv[i], "--prefix")) prefix = sp_cstr_as_str(argv[i + 1]);
    else if (sp_cstr_equal(argv[i], "--c")) c_out = sp_cstr_as_str(argv[i + 1]);
    else if (sp_cstr_equal(argv[i], "--ts")) ts_out = sp_cstr_as_str(argv[i + 1]);
    else return cli_usage();
  }

  if (sp_str_empty(prefix)) return cli_usage();
  if (sp_str_empty(c_out) && sp_str_empty(ts_out)) return cli_usage();

  sp_str_t endpoints;
  if (sp_io_read_file(mem, input, &endpoints) != SP_OK) {
    sp_log("spry: cannot read {}", sp_fmt_str(input));
    return 1;
  }

  spry_gen_opts_t opts = { .prefix = prefix, .endpoints = endpoints };

  if (!sp_str_empty(c_out)) {
    s32 rc = cli_emit(mem, opts, c_out, spry_gen_c);
    if (rc) return rc;
  }
  if (!sp_str_empty(ts_out)) {
    s32 rc = cli_emit(mem, opts, ts_out, spry_gen_ts);
    if (rc) return rc;
  }
  return 0;
}
