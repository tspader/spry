import type { Backend, HostIface } from "spry/backend";

export interface Mount {
  endpoints(endpointsJson: string): number;
  render(treeJson: string): number;
}

export async function mount<Node>(
  wasmUrl: string,
  makeBackend: (iface: HostIface) => Backend<Node>,
): Promise<Mount> {
  let instance!: WebAssembly.Instance;
  let memory!: WebAssembly.Memory;
  const encoder = new TextEncoder();
  const decoder = new TextDecoder();
  const readStr = (ptr: number, len: number): string =>
    decoder.decode(new Uint8Array(memory.buffer, ptr, len));

  const exported = <T>(name: string): T => instance.exports[name] as T;

  const writeBytes = (bytes: Uint8Array): number => {
    const alloc = exported<(len: number) => number>("alloc");
    const ptr = alloc(bytes.length);
    new Uint8Array(memory.buffer).set(bytes, ptr);
    return ptr;
  };

  const iface: HostIface = {
    dispatch: (token) => exported<(t: number) => void>("dispatch")(token),
    deliver: (token, outcome, body) => {
      const bytes = encoder.encode(body);
      const ptr = writeBytes(bytes);
      exported<(t: number, o: number, p: number, l: number) => void>("deliver")(token, outcome, ptr, bytes.length);
    },
  };

  const backend = makeBackend(iface);

  const handles: Node[] = [];
  const put = (node: Node): number => handles.push(node);
  const ref = (handle: number): Node => handles[handle - 1]!;

  const host = {
    capabilities: () => backend.capabilities(),
    create: (kind: number) => put(backend.createElement(kind)),
    set_direction: (handle: number, value: number) => backend.setDirection(ref(handle), value),
    set_gap: (handle: number, value: number) => backend.setGap(ref(handle), value),
    set_padding: (handle: number, value: number) => backend.setPadding(ref(handle), value),
    set_align: (handle: number, value: number) => backend.setAlign(ref(handle), value),
    set_justify: (handle: number, value: number) => backend.setJustify(ref(handle), value),
    set_text: (handle: number, ptr: number, len: number) => backend.setText(ref(handle), readStr(ptr, len)),
    set_href: (handle: number, ptr: number, len: number) => backend.setHref(ref(handle), readStr(ptr, len)),
    set_value: (handle: number, ptr: number, len: number) => backend.setValue(ref(handle), readStr(ptr, len)),
    set_name: (handle: number, ptr: number, len: number) => backend.setName(ref(handle), readStr(ptr, len)),
    set_placeholder: (handle: number, ptr: number, len: number) => backend.setPlaceholder(ref(handle), readStr(ptr, len)),
    append_child: (parent: number, child: number) => backend.appendChild(ref(parent), ref(child)),
    set_root: (handle: number) => backend.setRoot(ref(handle)),
    on_event: (handle: number, event: number, token: number) => backend.onEvent(ref(handle), event, token),
    invoke: (token: number, handlerPtr: number, handlerLen: number, bodyPtr: number, bodyLen: number) =>
      backend.invoke(token, readStr(handlerPtr, handlerLen), readStr(bodyPtr, bodyLen)),
    report: (token: number, ptr: number, len: number) => backend.report(token, readStr(ptr, len)),
    clear_children: (handle: number) => backend.clearChildren(ref(handle)),
    get_value: (handle: number, outPtr: number, cap: number): number => {
      const bytes = encoder.encode(backend.getValue(ref(handle)));
      const n = Math.min(bytes.length, cap);
      new Uint8Array(memory.buffer, outPtr, n).set(bytes.subarray(0, n));
      return n;
    },
    fatal: (ptr: number, len: number) => backend.fatal(readStr(ptr, len)),
  };

  const wasmBytes = await fetch(wasmUrl).then((r) => r.arrayBuffer());
  instance = (await WebAssembly.instantiate(wasmBytes, { host })).instance;
  memory = instance.exports.memory as WebAssembly.Memory;

  const endpoints = instance.exports.endpoints as (ptr: number, len: number) => number;
  const render = instance.exports.render as (ptr: number, len: number) => number;

  return {
    endpoints(endpointsJson: string): number {
      const bytes = encoder.encode(endpointsJson);
      return endpoints(writeBytes(bytes), bytes.length);
    },
    render(treeJson: string): number {
      handles.length = 0;
      const bytes = encoder.encode(treeJson);
      return render(writeBytes(bytes), bytes.length);
    },
  };
}
