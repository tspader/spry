#include "sp.h"
#include "spry/abi.h"
#include "spry/host.h"
#include "spry/backend/stub.h"

int main(int argc, char** argv) {
  if (argc < 4) {
    sp_log("usage: native-smoke <runtime.wasm> <tree.json> <endpoints.json>");
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

  sp_str_t endpoints;
  if (sp_io_read_file(mem, sp_cstr_as_str(argv[3]), &endpoints) != SP_OK) {
    sp_log("native-smoke: cannot read endpoints {}", sp_fmt_cstr(argv[3]));
    return 1;
  }

  s32 erc = wasm_host_endpoints(host, endpoints);
  sp_log("endpoints() -> {}", sp_fmt_int(erc));
  if (erc != 0) return 1;

  s32 rc = wasm_host_render(host, tree);
  sp_log("render() -> {}", sp_fmt_int(rc));
  if (rc != 0) return 1;

  host_iface_t iface = wasm_host_iface(host);

  sp_log("== ok patch ==");
  iface.dispatch(iface.ctx, 0);
  iface.dispatch(iface.ctx, 0);
  sp_str_t fragment = sp_str_lit("{\"kind\":\"text\",\"props\":{\"text\":\"patched\"}}");
  iface.deliver(iface.ctx, 0, DELIVER_OK, fragment);

  sp_log("== remote fault ==");
  iface.dispatch(iface.ctx, 0);
  sp_str_t fault = sp_str_lit("{\"code\":\"denied\",\"message\":\"not yours\"}");
  iface.deliver(iface.ctx, 0, DELIVER_FAULT, fault);

  sp_log("== intermediary ==");
  iface.dispatch(iface.ctx, 0);
  sp_str_t html = sp_str_lit("<html>502 Bad Gateway</html>");
  iface.deliver(iface.ctx, 0, 502, html);

  sp_log("== transport ==");
  iface.dispatch(iface.ctx, 0);
  iface.deliver(iface.ctx, 0, DELIVER_UNREACHABLE, sp_str_lit(""));

  sp_log("== input-sourced arg ==");
  iface.dispatch(iface.ctx, 1);

  return 0;
}
