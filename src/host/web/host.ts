import type { Backend } from "../../backend/backend.ts";
import { makeWasiShim } from "./wasi-shim.js";

export interface Mount {
  render(treeJson: string): number;
}

export async function mount<Node>(wasmUrl: string, backend: Backend<Node>): Promise<Mount> {
  const handles: Node[] = [];
  const put = (node: Node): number => handles.push(node);
  const ref = (handle: number): Node => handles[handle - 1]!;

  let memory!: WebAssembly.Memory;
  const decoder = new TextDecoder();
  const readStr = (ptr: number, len: number): string =>
    decoder.decode(new Uint8Array(memory.buffer, ptr, len));

  const host = {
    capabilities: () => backend.capabilities(),
    create: (kind: number) => put(backend.createElement(kind)),
    set_attr: (handle: number, attr: number, value: number) => backend.setAttr(ref(handle), attr, value),
    set_attr_str: (handle: number, attr: number, ptr: number, len: number) =>
      backend.setAttrStr(ref(handle), attr, readStr(ptr, len)),
    append_child: (parent: number, child: number) => backend.appendChild(ref(parent), ref(child)),
    set_root: (handle: number) => backend.setRoot(ref(handle)),
    fatal: (ptr: number, len: number) => backend.fatal(readStr(ptr, len)),
  };

  const shim = makeWasiShim();
  const bytes = await fetch(wasmUrl).then((r) => r.arrayBuffer());
  const { instance } = await WebAssembly.instantiate(bytes, { host, ...shim.imports });
  shim.setInstance(instance);
  memory = instance.exports.memory as WebAssembly.Memory;

  const alloc = instance.exports.alloc as (len: number) => number;
  const render = instance.exports.render as (ptr: number, len: number) => number;

  return {
    render(treeJson: string): number {
      handles.length = 0;
      const encoded = new TextEncoder().encode(treeJson);
      const ptr = alloc(encoded.length);
      new Uint8Array(memory.buffer).set(encoded, ptr);
      return render(ptr, encoded.length);
    },
  };
}
