import { readFileSync, writeFileSync } from "node:fs";

const SCHEMA_FILES = ["ui.jtd.json", "fault.jtd.json", "endpoints.jtd.json"];
const BUILDERS = { schema: "ui.jtd.json", node: "node", interaction: "interaction" };

type Schema = any;
const defs: Record<string, Schema> = {};
const roots: string[] = [];
for (const file of SCHEMA_FILES) {
  const jtd = JSON.parse(readFileSync(new URL(`../src/abi/${file}`, import.meta.url), "utf8"));
  for (const [name, d] of Object.entries<Schema>(jtd.definitions ?? {})) {
    if (defs[name]) throw new Error(`gen-types: duplicate definition '${name}'`);
    defs[name] = d;
  }
  if (!jtd.ref) throw new Error(`gen-types: ${file} root must be a ref`);
  roots.push(jtd.ref);
}

const pascal = (s: string) => s.split(/[_\s]+/).map((p) => p.charAt(0).toUpperCase() + p.slice(1)).join("");

const NUM_TYPES = ["int8", "uint8", "int16", "uint16", "int32", "uint32", "float32", "float64"];

const enumTypes: string[] = [];
const objectTypes: string[] = [];
const unionTypes: string[] = [];
const enumDone = new Map<string, string>();
const enumValues = new Map<string, string[]>();
const objectDone = new Set<string>();
const unionNames = new Set<string>();
const defDone = new Set<string>();

function registerEnum(key: string, values: string[]): string {
  const name = pascal(key);
  const sig = JSON.stringify(values);
  const prior = enumDone.get(name);
  if (prior !== undefined) {
    if (prior !== sig) throw new Error(`gen-types: enum '${name}' has conflicting value sets`);
    return name;
  }
  enumDone.set(name, sig);
  enumValues.set(name, values);
  enumTypes.push(`export type ${name} = ${values.map((v) => `"${v}"`).join(" | ")};`);
  return name;
}

function resolve(parent: string, key: string, sub: Schema): string {
  if (sub.ref) {
    ensureDef(sub.ref);
    return pascal(sub.ref);
  }
  if (sub.type === "string") return "string";
  if (sub.type === "boolean") return "boolean";
  if (NUM_TYPES.includes(sub.type)) return "number";
  if (sub.type) throw new Error(`gen-types: unsupported type '${sub.type}'`);
  if (sub.enum) return registerEnum(key, sub.enum);
  if (sub.elements) return `${resolve(parent, key, sub.elements)}[]`;
  if (sub.values) return `Record<string, ${resolve(parent, key, sub.values)}>`;
  if (sub.properties || sub.optionalProperties) {
    const name = pascal(`${parent}_${key}`);
    emitObject(name, sub);
    return name;
  }
  throw new Error(`gen-types: unsupported schema ${JSON.stringify(sub)}`);
}

function fieldEntries(schema: Schema): Array<[string, Schema, boolean]> {
  const out: Array<[string, Schema, boolean]> = [];
  for (const [key, sub] of Object.entries<Schema>(schema.properties ?? {})) out.push([key, sub, true]);
  for (const [key, sub] of Object.entries<Schema>(schema.optionalProperties ?? {})) out.push([key, sub, false]);
  return out;
}

function emitObject(name: string, schema: Schema) {
  if (objectDone.has(name)) return;
  objectDone.add(name);
  const lines = fieldEntries(schema).map(([key, sub, required]) => {
    return `  ${key}${required ? "" : "?"}: ${resolve(name, key, sub)};`;
  });
  objectTypes.push(`export type ${name} = {\n${lines.join("\n")}\n};`);
}

function emitUnion(name: string, schema: Schema) {
  unionNames.add(pascal(name));
  const members: string[] = [];
  for (const [tag, variant] of Object.entries<Schema>(schema.mapping ?? {})) {
    const fields = [`${schema.discriminator}: "${tag}"`];
    for (const [key, sub, required] of fieldEntries(variant)) {
      fields.push(`${key}${required ? "" : "?"}: ${resolve(tag, key, sub)}`);
    }
    members.push(`  | { ${fields.join("; ")} }`);
  }
  unionTypes.push(`export type ${pascal(name)} =\n${members.join("\n")};`);
}

function ensureDef(name: string) {
  if (defDone.has(name)) return;
  defDone.add(name);
  const d = defs[name];
  if (!d) throw new Error(`gen-types: unknown definition '${name}'`);
  if (d.discriminator) emitUnion(name, d);
  else if (d.values) {
    const v = resolve(name, "value", d.values);
    objectTypes.push(`export type ${pascal(name)} = Record<string, ${v}>;`);
  } else emitObject(pascal(name), d);
}

for (const root of roots) ensureDef(root);

const schemaOut = [
  "// Generated from src/abi/*.jtd.json by tools/gen-types.ts. Do not edit.",
  "",
  enumTypes.join("\n"),
  "",
  objectTypes.join("\n\n"),
  "",
  unionTypes.join("\n\n"),
  "",
].join("\n");

writeFileSync(new URL("../src/abi/schema.ts", import.meta.url), schemaOut);
console.log(`gen-types: ${enumTypes.length} enums, ${objectTypes.length} objects, ${unionTypes.length} unions -> src/abi/schema.ts`);

const builderTypes: string[] = [];
const builderFns: string[] = [];

const nodeDef = defs[BUILDERS.node];
const nodeName = pascal(BUILDERS.node);
const nodeDisc = nodeDef.discriminator;

for (const [tag, variant] of Object.entries<Schema>(nodeDef.mapping)) {
  const propsSchema = variant.properties?.props ?? variant.optionalProperties?.props;
  if (!propsSchema) throw new Error(`gen-types: builder variant '${tag}' has no props`);
  const propsFields = fieldEntries(propsSchema);
  const nodeFields = fieldEntries(variant).filter(([key]) => key !== "props" && key !== "children");
  const hasChildren = fieldEntries(variant).some(([key]) => key === "children");

  const optsName = `${pascal(tag)}Opts`;
  const optLines = [
    ...nodeFields.map(([key, sub, required]) => `  ${key}${required ? "" : "?"}: ${resolve(tag, key, sub)};`),
    ...propsFields.map(([key, sub, required]) => `  ${key}${required ? "" : "?"}: ${resolve(tag, key, sub)};`),
  ];
  builderTypes.push(`export type ${optsName} = {\n${optLines.join("\n")}\n};`);

  const requiredProps = propsFields.filter(([, , required]) => required);
  const sugar =
    requiredProps.length === 1 &&
    resolve(tag, requiredProps[0]![0], requiredProps[0]![1]) === "string" &&
    nodeFields.every(([, , required]) => !required);
  const allOptional = !sugar && [...nodeFields, ...propsFields].every(([, , required]) => !required);

  const nodeKeys = nodeFields.map(([key]) => key);
  const destructure = nodeKeys.length ? `const { ${nodeKeys.join(", ")}, ...props } = o;` : `const props = o;`;

  const params = sugar
    ? `opts: string | ${optsName}`
    : `opts: ${optsName}${allOptional ? " = {}" : ""}`;
  const childrenParam = hasChildren ? `, ...children: ${nodeName}[]` : "";

  const lines: string[] = [];
  lines.push(`export function ${tag}(${params}${childrenParam}): ${nodeName} {`);
  if (sugar) {
    lines.push(`  const o = typeof opts === "string" ? { ${requiredProps[0]![0]}: opts } : opts;`);
  } else {
    lines.push(`  const o = opts;`);
  }
  lines.push(`  ${destructure}`);
  lines.push(`  return {`);
  lines.push(`    ${nodeDisc}: "${tag}",`);
  for (const key of nodeKeys) {
    lines.push(`    ...(${key} !== undefined && { ${key} }),`);
  }
  lines.push(`    props,`);
  if (hasChildren) lines.push(`    ...(children.length > 0 && { children }),`);
  lines.push(`  };`);
  lines.push(`}`);
  builderFns.push(lines.join("\n"));
}

const interactionDef = defs[BUILDERS.interaction];
const interactionName = pascal(BUILDERS.interaction);
const interactionDisc = interactionDef.discriminator;

for (const [tag, variant] of Object.entries<Schema>(interactionDef.mapping)) {
  const fields = fieldEntries(variant);
  const optsName = `${pascal(tag)}Opts`;
  const optLines = fields.map(([key, sub, required]) => {
    const ts = resolve(tag, key, sub);
    const defaulted = required && enumValues.has(ts);
    return `  ${key}${required && !defaulted ? "" : "?"}: ${ts};`;
  });
  builderTypes.push(`export type ${optsName} = {\n${optLines.join("\n")}\n};`);

  const lines: string[] = [];
  lines.push(`export function ${tag}(opts: ${optsName}): ${interactionName} {`);
  lines.push(`  return {`);
  lines.push(`    ${interactionDisc}: "${tag}",`);
  for (const [key, sub, required] of fields) {
    const ts = resolve(tag, key, sub);
    if (required && enumValues.has(ts)) {
      lines.push(`    ${key}: opts.${key} ?? "${enumValues.get(ts)![0]}",`);
    } else if (required) {
      lines.push(`    ${key}: opts.${key},`);
    } else {
      lines.push(`    ...(opts.${key} !== undefined && { ${key}: opts.${key} }),`);
    }
  }
  lines.push(`  };`);
  lines.push(`}`);
  builderFns.push(lines.join("\n"));
}

const builderBody = [builderTypes.join("\n\n"), "", builderFns.join("\n\n")].join("\n");
const exported = [...enumDone.keys(), ...objectDone, ...unionNames];
const imports = exported.filter((name) => new RegExp(`\\b${name}\\b`).test(builderBody)).sort();

const uiOut = [
  `// Generated from src/abi/${BUILDERS.schema} by tools/gen-types.ts. Do not edit.`,
  `import type { ${imports.join(", ")} } from "./schema";`,
  "",
  builderBody,
  "",
].join("\n");

writeFileSync(new URL("../src/abi/ui.gen.ts", import.meta.url), uiOut);
console.log(`gen-types: ${builderFns.length} builders -> src/abi/ui.gen.ts`);
