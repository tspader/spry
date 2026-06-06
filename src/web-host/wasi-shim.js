// Minimal wasi_snapshot_preview1 shim for sp's wasm32-freestanding output.
// Only fd_write does real work; everything else is a success stub.
// Works in the browser and in Node (the WebAssembly API is identical).
//
//   const { imports, setInstance, getStdout } = makeWasiShim();
//   const { instance } = await WebAssembly.instantiate(bytes, imports);
//   setInstance(instance);
//   try { instance.exports._start(); } catch (e) { if (!e.__exit) throw e; }
//
export function makeWasiShim(write) {
  let instance = null;
  const ERRNO_SUCCESS = 0;
  const decoder = new TextDecoder();

  // Default sink: collect to a string. Override `write(fd, bytes)` to redirect.
  let captured = "";
  if (!write) {
    write = (fd, bytes) => { captured += decoder.decode(bytes); };
  }

  const mem = () => new DataView(instance.exports.memory.buffer);
  const bytes = (ptr, len) =>
    new Uint8Array(instance.exports.memory.buffer, ptr, len);

  const imports = {
    wasi_snapshot_preview1: {
      // fd_write(fd, iovs, iovs_len, nwritten) -> errno
      fd_write(fd, iovs, iovs_len, nwritten) {
        const dv = mem();
        let written = 0;
        for (let i = 0; i < iovs_len; i++) {
          const ptr = dv.getUint32(iovs + i * 8, true);
          const len = dv.getUint32(iovs + i * 8 + 4, true);
          if (len > 0) write(fd, bytes(ptr, len).slice());
          written += len;
        }
        dv.setUint32(nwritten, written, true);
        return ERRNO_SUCCESS;
      },

      // proc_exit(code) -> noreturn. Throw a tagged error so _start unwinds.
      proc_exit(code) {
        const e = new Error("proc_exit(" + code + ")");
        e.__exit = true;
        e.code = code;
        throw e;
      },

      // args/environ: report zero of each, success.
      args_sizes_get(argc, argv_buf_size) {
        const dv = mem();
        dv.setUint32(argc, 0, true);
        dv.setUint32(argv_buf_size, 0, true);
        return ERRNO_SUCCESS;
      },
      args_get() { return ERRNO_SUCCESS; },
      environ_sizes_get(count, buf_size) {
        const dv = mem();
        dv.setUint32(count, 0, true);
        dv.setUint32(buf_size, 0, true);
        return ERRNO_SUCCESS;
      },
      environ_get() { return ERRNO_SUCCESS; },
    },
  };

  return {
    imports,
    setInstance: (i) => { instance = i; },
    getStdout: () => captured,
  };
}
