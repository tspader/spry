const SCHEMA_FILES = ["ui.jtd.json", "fault.jtd.json", "endpoints.jtd.json"];
const outPath = new URL("../src/abi/schema.ts", import.meta.url);

type Schema = any;
const defs: Record<string, Schema> = {};
const roots: string[] = [];
for (const file of SCHEMA_FILES) {
  const jtd = JSON.parse(await Bun.file(new URL(`../src/abi/${file}`, import.meta.url)).text());
  for (const [name, d] of Object.entries<Schema>(jtd.definitions ?? {})) {
    if (defs[name]) throw new Error(`gen-zod: duplicate definition '${name}'`);
    defs[name] = d;
  }
  if (!jtd.ref) throw new Error(`gen-zod: ${file} root must be a {{ ref }}`);
  roots.push(jtd.ref);
}

const defKind: Record<string, "union" | "object" | "values"> = {};
for (const [name, d] of Object.entries(defs)) {
  defKind[name] = d.discriminator ? "union" : d.values ? "values" : "object";
}

const pascal = (s: string) => s.split(/[_\s]+/).map((p) => p.charAt(0).toUpperCase() + p.slice(1)).join("");

const enumConsts: string[] = [];
const objectConsts: string[] = [];
const enumDone = new Map<string, string>();
const objectDone = new Set<string>();
const defDone = new Set<string>();
const unionTs: string[] = [];
const unionZod: string[] = [];

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
  if (sub.type === "boolean") return { zod: "z.boolean()", ts: "boolean" };
  if (["int8", "uint8", "int16", "uint16", "int32", "uint32"].includes(sub.type)) {
    return { zod: "z.number().int()", ts: "number" };
  }
  if (sub.type === "float32" || sub.type === "float64") return { zod: "z.number()", ts: "number" };
  if (sub.type) throw new Error(`gen-zod: unsupported type '${sub.type}'`);
  if (sub.enum) {
    const name = registerEnum(key, sub.enum);
    return { zod: name, ts: `z.infer<typeof ${name}>` };
  }
  if (sub.elements) {
    const elem = resolve(parent, key, sub.elements);
    return { zod: `z.array(${elem.zod})`, ts: `${elem.ts}[]` };
  }
  if (sub.values) {
    const value = resolve(parent, key, sub.values);
    return { zod: `z.record(z.string(), ${value.zod})`, ts: `Record<string, ${value.ts}>` };
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
    const tsFields = [`${schema.discriminator}: "${tag}"`];
    const zodFields = [`${schema.discriminator}: z.literal("${tag}")`];
    for (const [key, sub, required] of fieldEntries(variant)) {
      const r = resolve(tag, key, sub);
      tsFields.push(`${key}${required ? "" : "?"}: ${r.ts}`);
      zodFields.push(`${key}: ${required ? r.zod : `${r.zod}.optional()`}`);
    }
    tsMembers.push(`  | { ${tsFields.join("; ")} }`);
    zodMembers.push(`    z.strictObject({ ${zodFields.join(", ")} }),`);
  }
  unionTs.push(`export type ${pascal(name)} =\n${tsMembers.join("\n")};`);
  unionZod.push(
    `export const ${pascal(name)}: z.ZodType<${pascal(name)}> = z.lazy(() =>\n` +
    `  z.discriminatedUnion("${schema.discriminator}", [\n${zodMembers.join("\n")}\n  ]),\n);`);
}

function ensureDef(name: string) {
  if (defDone.has(name)) return;
  defDone.add(name);
  const d = defs[name];
  if (!d) throw new Error(`gen-zod: unknown definition '${name}'`);
  if (d.discriminator) {
    emitUnion(name, d);
  } else if (d.values) {
    const value = resolve(name, "value", d.values);
    objectConsts.push(`export const ${pascal(name)} = z.record(z.string(), ${value.zod});`);
  } else {
    emitObject(pascal(name), d);
  }
}

for (const root of roots) ensureDef(root);

const out = [
  "// Generated from src/abi/ui.jtd.json by tools/gen-zod.ts. Do not edit.",
  'import { z } from "zod";',
  "",
  enumConsts.join("\n"),
  "",
  objectConsts.join("\n\n"),
  "",
  unionTs.join("\n\n"),
  "",
  unionZod.join("\n\n"),
  "",
].join("\n");

await Bun.write(outPath, out);
console.log(`gen-zod: ${enumConsts.length} enums, ${objectConsts.length} objects -> src/abi/schema.ts`);
