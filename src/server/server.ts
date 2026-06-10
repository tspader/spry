import { z } from "zod";
import { Endpoints, Node } from "spry/schema";

export type FaultCode =
  | "invalid"
  | "unauthenticated"
  | "denied"
  | "missing"
  | "conflict"
  | "failed"
  | "unavailable"
  | "timeout";

export type FaultIssue = { path: string; code: string };

const FAULT_STATUS: Record<FaultCode, number> = {
  invalid: 422,
  unauthenticated: 401,
  denied: 403,
  missing: 404,
  conflict: 409,
  failed: 500,
  unavailable: 503,
  timeout: 408,
};

export class Fault extends Error {
  code: FaultCode;
  issues?: FaultIssue[];

  constructor(code: FaultCode, message?: string, issues?: FaultIssue[]) {
    super(message ?? code);
    this.code = code;
    this.issues = issues;
  }
}

function faultResponse(fault: Fault): Response {
  const body = {
    code: fault.code,
    ...(fault.message && fault.message !== fault.code && { message: fault.message }),
    ...(fault.issues?.length && { issues: fault.issues }),
  };
  return new Response(JSON.stringify(body), {
    status: FAULT_STATUS[fault.code],
    headers: { "content-type": "application/json; charset=utf-8" },
  });
}

type EndpointsDoc = z.infer<typeof Endpoints>;

function checkArg(type: string, value: unknown): boolean {
  if (type === "string") return typeof value === "string";
  if (type === "boolean") return typeof value === "boolean";
  if (typeof value !== "number" || !Number.isFinite(value)) return false;
  if (type === "float32" || type === "float64") return true;
  return Number.isInteger(value);
}

function validateArgs(doc: EndpointsDoc, name: string, body: unknown): FaultIssue[] | Record<string, unknown> {
  const issues: FaultIssue[] = [];
  if (typeof body !== "object" || body === null || Array.isArray(body)) {
    return [{ path: "$", code: "type" }];
  }
  const args = doc[name]?.args ?? {};
  const required = args.properties ?? {};
  const optional = args.optionalProperties ?? {};
  const values = body as Record<string, unknown>;

  for (const key of Object.keys(values)) {
    if (!(key in required) && !(key in optional)) issues.push({ path: key, code: "unknown" });
  }
  for (const [key, decl] of Object.entries(required)) {
    if (!(key in values)) issues.push({ path: key, code: "missing" });
    else if (!checkArg(decl.type, values[key])) issues.push({ path: key, code: "type" });
  }
  for (const [key, decl] of Object.entries(optional)) {
    if (key in values && !checkArg(decl.type, values[key])) issues.push({ path: key, code: "type" });
  }

  return issues.length ? issues : values;
}

function json(value: unknown): Response {
  return new Response(JSON.stringify(value), {
    headers: { "content-type": "application/json; charset=utf-8" },
  });
}

function file(url: URL, contentType: string): Response | Promise<Response> {
  const f = Bun.file(url);
  return f.exists().then((ok) =>
    ok ? new Response(f, { headers: { "content-type": contentType } }) : new Response("not found", { status: 404 }),
  );
}

export type Handler = (args: Record<string, unknown>) => unknown | Promise<unknown>;

export type SpryAppOpts = {
  endpoints: unknown;
  handlers: Record<string, Handler>;
  tree: unknown | (() => unknown);
  webDir: URL;
  prefix?: string;
};

export function spryApp(opts: SpryAppOpts): { fetch(req: Request): Promise<Response> } {
  const doc = Endpoints.parse(opts.endpoints);
  const prefix = opts.prefix ?? "/api";

  for (const name of Object.keys(doc)) {
    if (!(name in opts.handlers)) throw new Error(`spry: endpoint '${name}' declared but no handler registered`);
  }
  for (const name of Object.keys(opts.handlers)) {
    if (!(name in doc)) throw new Error(`spry: handler '${name}' registered but not declared`);
  }

  async function invoke(name: string, req: Request): Promise<Response> {
    if (!(name in doc)) return faultResponse(new Fault("missing", `unknown endpoint '${name}'`));

    let body: unknown;
    try {
      body = await req.json();
    } catch {
      return faultResponse(new Fault("invalid", "malformed JSON body"));
    }

    const args = validateArgs(doc, name, body);
    if (Array.isArray(args)) return faultResponse(new Fault("invalid", "argument validation failed", args));

    try {
      const result = await opts.handlers[name]!(args);
      if (typeof result === "object" && result !== null && "kind" in result) {
        return json(Node.parse(result));
      }
      return json(result ?? {});
    } catch (err) {
      if (err instanceof Fault) return faultResponse(err);
      const message = err instanceof Error ? err.message : String(err);
      return faultResponse(new Fault("failed", message));
    }
  }

  async function handle(req: Request): Promise<Response> {
    const url = new URL(req.url);
    const path = url.pathname;

    if (req.method === "POST" && path.startsWith(`${prefix}/`)) {
      return invoke(path.slice(prefix.length + 1), req);
    }
    if (req.method !== "GET") return new Response("method not allowed", { status: 405 });

    if (path === "/") return file(new URL("index.html", opts.webDir), "text/html; charset=utf-8");
    if (path === "/host.bundle.js") return file(new URL("host.bundle.js", opts.webDir), "text/javascript; charset=utf-8");
    if (path === "/runtime.wasm") return file(new URL("runtime.wasm", opts.webDir), "application/wasm");
    if (path === "/endpoints") return json(doc);
    if (path === "/tree") {
      const tree = typeof opts.tree === "function" ? opts.tree() : opts.tree;
      return json(Node.parse(tree));
    }

    return new Response("not found", { status: 404 });
  }

  return { fetch: handle };
}
