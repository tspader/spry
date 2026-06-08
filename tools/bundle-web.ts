import { $ } from "bun";

const outDir = ".build/store/web/example";
await $`mkdir -p ${outDir}`;

const out = await Bun.build({
  entrypoints: ["example/web/main.ts"],
  outdir: outDir,
  target: "browser",
});
if (!out.success) {
  for (const message of out.logs) console.error(message);
  throw new Error("web bundle failed");
}

await $`mv ${outDir}/main.js ${outDir}/host.bundle.js`;
await $`cp example/web/index.html ${outDir}/index.html`;
console.log("bundle-web: done");
