import { $ } from "bun";

const root = new URL("../", import.meta.url);
const p = (rel: string): string => new URL(rel, root).pathname;

async function genAbi(): Promise<void> {
  await $`bun run ${p("tools/gen-abi.ts")}`;
}

async function buildWasm(): Promise<void> {
  await $`mkdir -p ${p(".cache")}`;
  await $`zig cc --target=wasm32-freestanding -nostdlib -fno-sanitize=undefined -I${p("vendor")} -I${p("src/abi")} -I${p("src/runtime")} -Wl,--no-entry -Wl,--export-dynamic -o ${p(".cache/runtime.wasm")} ${p("src/runtime/runtime.c")}`;
}

async function bundleWeb(): Promise<void> {
  await $`mkdir -p ${p("build/web")}`;
  const out = await Bun.build({
    entrypoints: [p("example/web/main.ts")],
    outdir: p("build/web"),
    target: "browser",
  });
  if (!out.success) {
    for (const message of out.logs) console.error(message);
    throw new Error("web bundle failed");
  }
  await $`mv ${p("build/web/main.js")} ${p("build/web/host.bundle.js")}`;
  await $`cp ${p("example/web/index.html")} ${p("build/web/index.html")}`;
  await $`cp ${p(".cache/runtime.wasm")} ${p("build/web/runtime.wasm")}`;
}

async function pkgConfig(flag: string): Promise<string[]> {
  const out = (await $`pkg-config ${flag} gtk4`.text()).trim();
  return out.length ? out.split(/\s+/) : [];
}

async function linkNative(): Promise<void> {
  await $`mkdir -p ${p("build/gtk")}`;
  const cflags = await pkgConfig("--cflags");
  const libs = await pkgConfig("--libs");
  await $`zig cc ${cflags} -I${p("vendor")} -I${p("src/abi")} -I${p("src/backend")} -I${p("src/host/native")} -I${p("src/backend/gtk")} ${p("src/host/native/wasm_host.c")} ${p("src/backend/gtk/gtk_backend.c")} ${p("example/native/main.c")} -lwasmer ${libs} -o ${p("build/gtk/app")}`;
}

async function linkSmoke(): Promise<void> {
  await $`mkdir -p ${p(".cache")}`;
  await $`zig cc -I${p("vendor")} -I${p("src/abi")} -I${p("src/backend")} -I${p("src/host/native")} -I${p("src/backend/stub")} ${p("src/host/native/wasm_host.c")} ${p("src/backend/stub/stub_backend.c")} ${p("example/native/smoke.c")} -lwasmer -o ${p(".cache/native-smoke")}`;
}

const cmd = process.argv[2] ?? "all";
switch (cmd) {
  case "abi":    await genAbi(); break;
  case "wasm":   await buildWasm(); break;
  case "web":    await genAbi(); await buildWasm(); await bundleWeb(); break;
  case "native": await buildWasm(); await linkNative(); break;
  case "smoke":  await buildWasm(); await linkSmoke(); break;
  case "all":    await genAbi(); await buildWasm(); await bundleWeb(); await linkNative(); break;
  default: throw new Error(`build: unknown target '${cmd}'`);
}
console.log(`build: ${cmd} done`);
