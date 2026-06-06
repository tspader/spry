import { $ } from "bun";

const root = new URL("../", import.meta.url);
const p = (rel: string): string => new URL(rel, root).pathname;

async function genAbi(): Promise<void> {
  await $`bun run ${p("tools/gen-abi.ts")}`;
}

async function buildWasm(): Promise<void> {
  await $`mkdir -p ${p(".cache")} ${p("build/web")}`;
  await $`zig cc --target=wasm32-freestanding -nostdlib -fno-sanitize=undefined -I${p("vendor")} -I${p("abi")} -I${p("src/runtime")} -Wl,--no-entry -Wl,--export-dynamic -o ${p(".cache/runtime.wasm")} ${p("src/runtime/runtime.c")}`;
  await $`cp ${p(".cache/runtime.wasm")} ${p("build/web/runtime.wasm")}`;
}

async function buildWeb(): Promise<void> {
  await $`mkdir -p ${p("build/web")}`;
  const out = await Bun.build({
    entrypoints: [p("src/web-host/main.ts")],
    outdir: p("build/web"),
    target: "browser",
  });
  if (!out.success) {
    for (const message of out.logs) console.error(message);
    throw new Error("web bundle failed");
  }
  await $`mv ${p("build/web/main.js")} ${p("build/web/host.bundle.js")}`;
  await $`cp ${p("src/web-host/index.html")} ${p("build/web/index.html")}`;
}

const cmd = process.argv[2] ?? "all";
if (cmd === "all" || cmd === "abi") await genAbi();
if (cmd === "all" || cmd === "wasm") await buildWasm();
if (cmd === "all" || cmd === "web") await buildWeb();
console.log(`build: ${cmd} done`);
