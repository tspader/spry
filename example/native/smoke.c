#include "sp.h"
#include "spry/host.h"
#include "spry/backend/stub.h"

int main(int argc, char** argv) {
  if (argc < 3) {
    sp_log("usage: native-smoke <runtime.wasm> <tree.json>");
    return 1;
  }

  sp_mem_t mem = sp_mem_os_new();

  wasm_host_t* host = wasm_host_new(mem, sp_cstr_as_str(argv[1]));
  if (!host) return 1;

  backend_t backend = stub_backend_make(mem);
  wasm_host_set_backend(host, &backend);

  sp_str_t tree;
  if (sp_io_read_file(mem, sp_cstr_as_str(argv[2]), &tree) != SP_OK) {
    sp_log("native-smoke: cannot read tree {}", sp_fmt_cstr(argv[2]));
    return 1;
  }

  s32 rc = wasm_host_render(host, tree);
  sp_log("render() -> {}", sp_fmt_int(rc));
  return rc == 0 ? 0 : 1;
}
