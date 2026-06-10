import { readFileSync, writeFileSync } from "node:fs";

const [input, output, symbol] = process.argv.slice(2);
if (!input || !output || !symbol) {
  console.error("usage: embed-wasm.ts <input> <output.c> <symbol>");
  process.exit(1);
}

const bytes = readFileSync(input);
const lines: string[] = [];
for (let i = 0; i < bytes.length; i += 16) {
  const chunk = [...bytes.subarray(i, i + 16)].map((b) => `0x${b.toString(16).padStart(2, "0")}`);
  lines.push(`  ${chunk.join(", ")},`);
}

const out = [
  `const unsigned char ${symbol}[] = {`,
  ...lines,
  `};`,
  `const unsigned int ${symbol}_len = ${bytes.length};`,
  ``,
].join("\n");

writeFileSync(output, out);
console.log(`embed-wasm: ${bytes.length} bytes -> ${output}`);
