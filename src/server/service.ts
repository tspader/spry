import type { FaultIssue } from "spry/schema";

type ServiceExports = {
  memory: WebAssembly.Memory;
  begin(): void;
  alloc(len: number): number;
  result_ptr(): number;
  result_len(): number;
  endpoints(ptr: number, len: number): number;
  validate(namePtr: number, nameLen: number, bodyPtr: number, bodyLen: number): number;
  fault(codePtr: number, codeLen: number, msgPtr: number, msgLen: number, issuesPtr: number, issuesLen: number): number;
  fragment(ptr: number, len: number): number;
};

export type ServiceFault = { status: number; fault: string };

export type Service = {
  endpoints(json: string): void;
  validate(name: string, body: string): ServiceFault | null;
  fault(code: string, message?: string, issues?: FaultIssue[]): ServiceFault;
  fragment(json: string): string | null;
};

// One wasm instance backs every call. begin() resets the per-call arena that holds both
// the inputs written via alloc() and the result buffer, so each method must write inputs,
// call, and read the result synchronously — no await between begin() and result().
export async function loadService(): Promise<Service> {
  const bytes = await Bun.file(new URL("../assets/service.wasm", import.meta.url)).arrayBuffer();
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports as unknown as ServiceExports;
  const encoder = new TextEncoder();
  const decoder = new TextDecoder();

  const write = (s: string): [number, number] => {
    const data = encoder.encode(s);
    if (data.length === 0) return [0, 0];
    const ptr = ex.alloc(data.length);
    new Uint8Array(ex.memory.buffer).set(data, ptr);
    return [ptr, data.length];
  };

  const result = (): string => decoder.decode(new Uint8Array(ex.memory.buffer, ex.result_ptr(), ex.result_len()));

  return {
    endpoints(json) {
      ex.begin();
      const [ptr, len] = write(json);
      if (ex.endpoints(ptr, len) !== 0) throw new Error(`spry: invalid endpoints document: ${result()}`);
    },

    validate(name, body) {
      ex.begin();
      const [namePtr, nameLen] = write(name);
      const [bodyPtr, bodyLen] = write(body);
      const status = ex.validate(namePtr, nameLen, bodyPtr, bodyLen);
      if (status === 0) return null;
      return { status, fault: result() };
    },

    fault(code, message, issues) {
      ex.begin();
      const [codePtr, codeLen] = write(code);
      const [msgPtr, msgLen] = write(message && message !== code ? message : "");
      const [issuesPtr, issuesLen] = write(issues?.length ? JSON.stringify(issues) : "");
      const status = ex.fault(codePtr, codeLen, msgPtr, msgLen, issuesPtr, issuesLen);
      return { status, fault: result() };
    },

    fragment(json) {
      ex.begin();
      const [ptr, len] = write(json);
      if (ex.fragment(ptr, len) !== 0) return result();
      return null;
    },
  };
}
