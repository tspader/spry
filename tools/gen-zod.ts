const inPath = new URL("../src/abi/ui.jtd.json", import.meta.url);
const outPath = new URL("../src/abi/schema.ts", import.meta.url);

type Schema = any;
const jtd = JSON.parse(await Bun.file(inPath).text());
const defs: Record<string, Schema> = jtd.definitions ?? {};

const defKind: Record<string, "union" | "object"> = {};
for (const [name, d] of Object.entries(defs)) {
  defKind[name] = d.discriminator ? "union" : "object";
}

const pascal = (s: string) => s.split(/[_\s]+/).map((p) => p.charAt(0).toUpperCase() + p.slice(1)).join("");

const enumConsts: string[] = [];
const objectConsts: string[] = [];
const enumDone = new Map<string, string>();
const objectDone = new Set<string>();
const defDone = new Set<string>();
let unionTs = "";
let unionZod = "";

function registerEnum(key: string, values: string[]) {
  const name = pascal(key);
  const sig = JSON.stringify(values);
  const prior = enumDone.get(name);
  if (prior !== undefined) {
    if (prior !== sig) throw new Error(`gen-zod: enum '${name}' has conflicting value sets`);
    return name;
  }
  enumDone.set(name, sig);
  enumConsts.push(`export const ${name} = z.enum([${values.map((v) => `"${v}"`).join(", ")}]);`);
  return name;
}

type Resolved = { zod: string; ts: string };

function resolve(parent: string, key: string, sub: Schema): Resolved {
  if (sub.ref) {
    ensureDef(sub.ref);
    if (defKind[sub.ref] === "union") return { zod: pascal(sub.ref), ts: pascal(sub.ref) };
    const name = pascal(sub.ref);
    return { zod: name, ts: `z.infer<typeof ${name}>` };
  }
  if (sub.type === "string") return { zod: "z.string()", ts: "string" };
  if (sub.type === "int32") return { zod: "z.number().int()", ts: "number" };
  if (sub.type === "boolean") return { zod: "z.boolean()", ts: "boolean" };
  if (sub.type) throw new Error(`gen-zod: unsupported type '${sub.type}'`);
  if (sub.enum) {
    const name = registerEnum(key, sub.enum);
    return { zod: name, ts: `z.infer<typeof ${name}>` };
  }
  if (sub.elements) {
    const elem = resolve(parent, key, sub.elements);
    return { zod: `z.array(${elem.zod})`, ts: `${elem.ts}[]` };
  }
  if (sub.properties || sub.optionalProperties) {
    const name = pascal(`${parent}_${key}`);
    emitObject(name, sub);
    return { zod: name, ts: `z.infer<typeof ${name}>` };
  }
  throw new Error(`gen-zod: unsupported schema ${JSON.stringify(sub)}`);
}

function fieldEntries(schema: Schema): Array<[string, Schema, boolean]> {
  const out: Array<[string, Schema, boolean]> = [];
  for (const [key, sub] of Object.entries(schema.properties ?? {})) out.push([key, sub, true]);
  for (const [key, sub] of Object.entries(schema.optionalProperties ?? {})) out.push([key, sub, false]);
  return out;
}

function emitObject(name: string, schema: Schema) {
  if (objectDone.has(name)) return;
  objectDone.add(name);
  const lines = fieldEntries(schema).map(([key, sub, required]) => {
    const r = resolve(name, key, sub);
    return `  ${key}: ${required ? r.zod : `${r.zod}.optional()`},`;
  });
  objectConsts.push(`export const ${name} = z.strictObject({\n${lines.join("\n")}\n});`);
}

function emitUnion(name: string, schema: Schema) {
  const tsMembers: string[] = [];
  const zodMembers: string[] = [];
  for (const [tag, variant] of Object.entries<Schema>(schema.mapping ?? {})) {
    const tsFields = [`kind: "${tag}"`];
    const zodFields = [`kind: z.literal("${tag}")`];
    for (const [key, sub, required] of fieldEntries(variant)) {
      const r = resolve(tag, key, sub);
      tsFields.push(`${key}${required ? "" : "?"}: ${r.ts}`);
      zodFields.push(`${key}: ${required ? r.zod : `${r.zod}.optional()`}`);
    }
    tsMembers.push(`  | { ${tsFields.join("; ")} }`);
    zodMembers.push(`    z.strictObject({ ${zodFields.join(", ")} }),`);
  }
  unionTs = `export type ${pascal(name)} =\n${tsMembers.join("\n")};`;
  unionZod =
    `export const ${pascal(name)}: z.ZodType<${pascal(name)}> = z.lazy(() =>\n` +
    `  z.discriminatedUnion("${schema.discriminator}", [\n${zodMembers.join("\n")}\n  ]),\n);`;
}

function ensureDef(name: string) {
  if (defDone.has(name)) return;
  defDone.add(name);
  const d = defs[name];
  if (!d) throw new Error(`gen-zod: unknown definition '${name}'`);
  if (d.discriminator) {
    emitUnion(name, d);
  } else {
    emitObject(pascal(name), d);
  }
}

if (!jtd.ref) throw new Error("gen-zod: schema root must be a { ref }");
ensureDef(jtd.ref);

const out = [
  "// Generated from src/abi/ui.jtd.json by tools/gen-zod.ts. Do not edit.",
  'import { z } from "zod";',
  "",
  enumConsts.join("\n"),
  "",
  objectConsts.join("\n\n"),
  "",
  unionTs,
  "",
  unionZod,
  "",
].join("\n");

await Bun.write(outPath, out);
console.log(`gen-zod: ${enumConsts.length} enums, ${objectConsts.length} objects -> src/abi/schema.ts`);
