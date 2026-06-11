import type { Endpoints, FaultIssue } from "spry/schema";
import { loadService } from "./service";
import type { ServiceFault } from "./service";

export type FaultCode =
  | "invalid"
  | "unauthenticated"
  | "denied"
  | "missing"
  | "conflict"
  | "failed"
  | "unavailable"
  | "timeout";

export class Fault extends Error {
  code: FaultCode;
  issues?: FaultIssue[];

  constructor(code: FaultCode, message?: string, issues?: FaultIssue[]) {
    super(message ?? code);
    this.code = code;
    this.issues = issues;
  }
}

const JSON_HEADERS = { "content-type": "application/json; charset=utf-8" };

function json(text: string): Response {
  return new Response(text, { headers: JSON_HEADERS });
}

function faultResponse(fault: ServiceFault): Response {
  return new Response(fault.fault, { status: fault.status, headers: JSON_HEADERS });
}

function file(url: URL, contentType: string): Promise<Response> {
  const f = Bun.file(url);
  return f.exists().then((ok) =>
    ok ? new Response(f, { headers: { "content-type": contentType } }) : new Response("not found", { status: 404 }),
  );
}

export type Handler = (args: any) => unknown | Promise<unknown>;

export type SpryAppOpts = {
  endpoints: unknown;
  handlers: Record<string, Handler>;
  tree: unknown | (() => unknown);
  prefix?: string;
};

export async function spryApp(opts: SpryAppOpts): Promise<{ fetch(req: Request): Promise<Response> }> {
  const service = await loadService();
  const endpointsJson = JSON.stringify(opts.endpoints);
  service.endpoints(endpointsJson);

  const doc = opts.endpoints as Endpoints;
  for (const name of Object.keys(doc)) {
    if (!(name in opts.handlers)) throw new Error(`spry: endpoint '${name}' declared but no handler registered`);
  }
  for (const name of Object.keys(opts.handlers)) {
    if (!(name in doc)) throw new Error(`spry: handler '${name}' registered but not declared`);
  }

  const prefix = opts.prefix ?? "/api";
  const clientDir = new URL("../client/", import.meta.url);
  const assetsDir = new URL("../assets/", import.meta.url);

  async function invoke(name: string, req: Request): Promise<Response> {
    const body = await req.text();
    const invalid = service.validate(name, body);
    if (invalid) return faultResponse(invalid);

    const args = JSON.parse(body) as Record<string, unknown>;
    try {
      const result = await opts.handlers[name]!(args);
      if (Array.isArray(result) && result.some((n) => typeof n === "object" && n !== null && "kind" in n)) {
        throw new Error("handler returned a fragment; return a single root node");
      }
      if (typeof result === "object" && result !== null && "kind" in result) {
        const fragment = JSON.stringify(result);
        const error = service.fragment(fragment);
        if (error) throw new Error(`invalid fragment: ${error}`);
        return json(fragment);
      }
      return json(JSON.stringify(result ?? {}));
    } catch (err) {
      if (err instanceof Fault) return faultResponse(service.fault(err.code, err.message, err.issues));
      const message = err instanceof Error ? err.message : String(err);
      return faultResponse(service.fault("failed", message));
    }
  }

  async function handle(req: Request): Promise<Response> {
    const url = new URL(req.url);
    const path = url.pathname;

    if (req.method === "POST" && path.startsWith(`${prefix}/`)) {
      return invoke(path.slice(prefix.length + 1), req);
    }
    if (req.method !== "GET") return new Response("method not allowed", { status: 405 });

    if (path === "/") return file(new URL("index.html", clientDir), "text/html; charset=utf-8");
    if (path === "/host.bundle.js") return file(new URL("host.bundle.js", assetsDir), "text/javascript; charset=utf-8");
    if (path === "/runtime.wasm") return file(new URL("runtime.wasm", assetsDir), "application/wasm");
    if (path === "/endpoints") return json(endpointsJson);
    if (path === "/tree") {
      const tree = typeof opts.tree === "function" ? (opts.tree as () => unknown)() : opts.tree;
      const treeJson = JSON.stringify(tree);
      const error = service.fragment(treeJson);
      if (error) throw new Error(`spry: invalid tree: ${error}`);
      return json(treeJson);
    }

    return new Response("not found", { status: 404 });
  }

  return { fetch: handle };
}
