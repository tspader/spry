const abiPath = new URL("../include/spry/abi.h", import.meta.url);
const outPath = new URL("../src/abi/abi.gen.ts", import.meta.url);

const src = await Bun.file(abiPath).text();

function evalExpr(expr: string): number {
  const e = expr.trim();
  const shift = e.match(/^(\d+)\s*<<\s*(\d+)$/);
  if (shift) return Number(shift[1]) << Number(shift[2]);
  if (/^\d+$/.test(e)) return Number(e);
  throw new Error(`gen-abi: cannot evaluate enum value '${expr}'`);
}

const consts: Array<[string, number]> = [];
const enumRe = /typedef\s+enum\s*\{([^}]*)\}\s*\w+_t\s*;/g;
let m: RegExpExecArray | null;
while ((m = enumRe.exec(src)) !== null) {
  const body = m[1]!;
  for (const part of body.split(",")) {
    const assign = part.match(/^\s*([A-Z][A-Z0-9_]*)\s*=\s*([^,]+)\s*$/);
    if (!assign) continue;
    consts.push([assign[1]!, evalExpr(assign[2]!)]);
  }
}

const lines = [
  "// Generated from abi/abi.h by tools/gen-abi.ts. Do not edit.",
  ...consts.map(([name, value]) => `export const ${name} = ${value};`),
  "",
];

await Bun.write(outPath, lines.join("\n"));
console.log(`gen-abi: wrote ${consts.length} constants to src/abi/abi.gen.ts`);
