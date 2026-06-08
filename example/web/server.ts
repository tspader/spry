import { Hono, type Context } from "hono";

const root = new URL("../../", import.meta.url);
const webDir = new URL("build/web/", root);
const treeFile = new URL("example/ui/tree.json", root);

const serve = (url: URL, contentType: string) => async (c: Context) => {
  const f = Bun.file(url);
  if (!(await f.exists())) return c.notFound();
  return new Response(f, { headers: { "content-type": contentType } });
};

const app = new Hono();
app.get("/", serve(new URL("index.html", webDir), "text/html; charset=utf-8"));
app.get("/host.bundle.js", serve(new URL("host.bundle.js", webDir), "text/javascript; charset=utf-8"));
app.get("/runtime.wasm", serve(new URL("runtime.wasm", webDir), "application/wasm"));
app.get("/tree", serve(treeFile, "application/json; charset=utf-8"));

app.post("/api/greet", async (c) => {
  const body = await c.req.text();
  const name = new URLSearchParams(body).get("name") || "stranger";
  return c.json({ kind: "text", props: { text: `Hello, ${name}!` } });
});

export default { port: 3000, fetch: app.fetch };
