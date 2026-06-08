const wasmPath = ".build/store/test/yyjson-probe.wasm";

const buf = await Bun.file(wasmPath).arrayBuffer();
const { instance } = await WebAssembly.instantiate(buf, {});
const probe = instance.exports.probe as () => number;

const got = probe();
const ok = got === 5;
console.log(`yyjson-wasm: probe() = ${got} (expect 5) -> ${ok ? "ok" : "FAIL"}`);
process.exit(ok ? 0 : 1);
