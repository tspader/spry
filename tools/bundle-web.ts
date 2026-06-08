import { $ } from "bun";

// Bundles the web host into build/web/. Run from the repo root (the Makefile
// invokes it that way); paths below are cwd-relative.
await $`mkdir -p build/web`;

const out = await Bun.build({
  entrypoints: ["example/web/main.ts"],
  outdir: "build/web",
  target: "browser",
});
if (!out.success) {
  for (const message of out.logs) console.error(message);
  throw new Error("web bundle failed");
}

await $`mv build/web/main.js build/web/host.bundle.js`;
await $`cp example/web/index.html build/web/index.html`;
await $`cp .cache/runtime.wasm build/web/runtime.wasm`;
console.log("bundle-web: done");
