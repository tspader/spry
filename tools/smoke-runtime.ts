import { makeWasiShim } from "../src/web-host/wasi-shim.js";

const wasmPath = new URL("../.cache/runtime.wasm", import.meta.url);
const treePath = new URL("../abi/tree.example.json", import.meta.url);

const bytes = await Bun.file(wasmPath).arrayBuffer();
const tree = await Bun.file(treePath).text();

let memory!: WebAssembly.Memory;
const dec = new TextDecoder();
const readStr = (ptr: number, len: number) => dec.decode(new Uint8Array(memory.buffer, ptr, len));

const log: string[] = [];
let nextHandle = 1;

const host = {
  capabilities: () => 0b1111,
  create: (kind: number) => { const h = nextHandle++; log.push(`create kind=${kind} -> #${h}`); return h; },
  set_attr: (h: number, attr: number, v: number) => log.push(`set_attr #${h} attr=${attr} val=${v}`),
  set_attr_str: (h: number, attr: number, ptr: number, len: number) =>
    log.push(`set_attr_str #${h} attr=${attr} "${readStr(ptr, len)}"`),
  append_child: (p: number, c: number) => log.push(`append #${c} -> #${p}`),
  set_root: (h: number) => log.push(`set_root #${h}`),
  fatal: (ptr: number, len: number) => log.push(`FATAL: ${readStr(ptr, len)}`),
};

const shim = makeWasiShim();
const { instance } = await WebAssembly.instantiate(bytes, { host, ...shim.imports });
shim.setInstance(instance);
memory = instance.exports.memory as WebAssembly.Memory;

const alloc = instance.exports.alloc as (n: number) => number;
const render = instance.exports.render as (p: number, l: number) => number;

const enc = new TextEncoder().encode(tree);
const ptr = alloc(enc.length);
new Uint8Array(memory.buffer).set(enc, ptr);
const rc = render(ptr, enc.length);

console.log(log.join("\n"));
console.log(`\nrender() returned ${rc}`);
if (rc !== 0) process.exit(1);
