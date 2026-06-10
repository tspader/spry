import { $ } from "bun";

const outDir = "src/assets";
await $`mkdir -p ${outDir}`;

const out = await Bun.build({
  entrypoints: ["src/client/main.ts"],
  outdir: outDir,
  target: "browser",
});
if (!out.success) {
  for (const message of out.logs) console.error(message);
  throw new Error("web bundle failed");
}

await $`mv ${outDir}/main.js ${outDir}/host.bundle.js`;
console.log("bundle-web: done");
