import { readFileSync, writeFileSync } from "node:fs";
import { NODE, INTERACTION, SUGAR, CONTENT, AUTO_TEXT, INVOKE_SUGAR } from "./ui-config";

const SCHEMA_FILES = ["ui.jtd.json", "fault.jtd.json", "endpoints.jtd.json"];
const BUILDERS = { schema: "ui.jtd.json", node: NODE, interaction: INTERACTION };
const LOOSE_VALUE = "string | number | boolean";

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
const jsxKinds: Array<{ tag: string; optsName: string; sugarProp: string | null; hasChildren: boolean }> = [];

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

  const content = CONTENT[tag];
  if (content !== undefined) {
    const sub = propsFields.find(([key]) => key === content);
    if (!sub || resolve(tag, content, sub[1]) !== "string") throw new Error(`gen-types: content prop '${content}' of '${tag}' is not a string prop`);
  }
  const sugar = content !== undefined;
  const allOptional = !sugar && [...nodeFields, ...propsFields].every(([, , required]) => !required);
  jsxKinds.push({ tag, optsName, sugarProp: content ?? null, hasChildren });

  const nodeKeys = nodeFields.map(([key]) => key);
  const destructure = nodeKeys.length ? `const { ${nodeKeys.join(", ")}, ...props } = o;` : `const props = o;`;

  const params = sugar
    ? `opts: string | ${optsName}`
    : `opts: ${optsName}${allOptional ? " = {}" : ""}`;
  const childrenParam = hasChildren ? `, ...children: ${nodeName}[]` : "";

  const lines: string[] = [];
  lines.push(`export function ${tag}(${params}${childrenParam}): ${nodeName} {`);
  if (sugar) {
    lines.push(`  const o = typeof opts === "string" ? { ${content}: opts } : opts;`);
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

for (const [name, sugar] of Object.entries(SUGAR)) {
  const of = jsxKinds.find((k) => k.tag === sugar.of);
  if (!of) throw new Error(`gen-types: sugar '${name}' names unknown kind '${sugar.of}'`);
  if (!of.hasChildren) throw new Error(`gen-types: sugar '${name}' kind '${sugar.of}' has no children`);
  const presetKeys = Object.keys(sugar.preset).map((key) => `"${key}"`).join(" | ");
  const preset = Object.entries(sugar.preset).map(([key, value]) => `${key}: "${value}"`).join(", ");
  builderFns.push(
    `export function ${name}(opts: Omit<${of.optsName}, ${presetKeys}> = {}, ...children: ${nodeName}[]): ${nodeName} {\n` +
      `  return ${sugar.of}({ ...opts, ${preset} }, ...children);\n` +
      `}`,
  );
}

const interactionDef = defs[BUILDERS.interaction];
const interactionName = pascal(BUILDERS.interaction);
const interactionDisc = interactionDef.discriminator;

const looseValues = (sub: Schema) => sub.values?.type === "string";

for (const [tag, variant] of Object.entries<Schema>(interactionDef.mapping)) {
  const fields = fieldEntries(variant);
  const optsName = `${pascal(tag)}Opts`;
  const optLines = fields.map(([key, sub, required]) => {
    const ts = looseValues(sub) ? `Record<string, ${LOOSE_VALUE}>` : resolve(tag, key, sub);
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
    if (looseValues(sub)) {
      lines.push(
        `    ...(opts.${key} !== undefined && { ${key}: Object.fromEntries(Object.entries(opts.${key}).map(([k, v]) => [k, String(v)])) }),`,
      );
    } else if (required && enumValues.has(ts)) {
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

for (const [name, sugar] of Object.entries(INVOKE_SUGAR)) {
  const variant = interactionDef.mapping[sugar.of];
  if (!variant) throw new Error(`gen-types: invoke sugar '${name}' names unknown variant '${sugar.of}'`);
  const fields = new Map(fieldEntries(variant).map(([key, sub, required]) => [key, { sub, required }]));
  const params: string[] = [];
  const passed: string[] = [];
  for (const arg of sugar.args) {
    const field = fields.get(arg);
    if (!field) throw new Error(`gen-types: invoke sugar '${name}' arg '${arg}' is not a field of '${sugar.of}'`);
    if (looseValues(field.sub)) {
      params.push(`${arg}?: Record<string, ${LOOSE_VALUE}>`);
      passed.push(`...(${arg} !== undefined && { ${arg} })`);
    } else {
      params.push(`${arg}: ${resolve(sugar.of, arg, field.sub)}`);
      passed.push(arg);
    }
  }
  for (const [key, value] of Object.entries(sugar.preset)) {
    const field = fields.get(key);
    if (!field?.sub.enum?.includes(value)) throw new Error(`gen-types: invoke sugar '${name}' preset '${key}=${value}' is not an enum value`);
    passed.push(`${key}: "${value}"`);
  }
  builderFns.push(
    `export function ${name}(${params.join(", ")}): ${interactionName} {\n` +
      `  return ${sugar.of}({ ${passed.join(", ")} });\n` +
      `}`,
  );
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

const autoTextKind = jsxKinds.find((k) => k.tag === AUTO_TEXT.kind);
if (!autoTextKind || autoTextKind.sugarProp !== AUTO_TEXT.prop) throw new Error(`gen-types: autoText '${AUTO_TEXT.kind}.${AUTO_TEXT.prop}' is not a content prop`);

const intrinsicRows: string[] = [];
const tableRows: string[] = [];

for (const k of jsxKinds) {
  if (k.hasChildren) {
    intrinsicRows.push(`  ${k.tag}: ${k.optsName} & { children?: JsxChild };`);
  } else if (k.sugarProp) {
    intrinsicRows.push(
      `  ${k.tag}: (${k.optsName} & { children?: never }) | (Omit<${k.optsName}, "${k.sugarProp}"> & { children: JsxText });`,
    );
  } else {
    intrinsicRows.push(`  ${k.tag}: ${k.optsName} & { children?: never };`);
  }
  const fields = [`kind: "${k.tag}"`, `container: ${k.hasChildren}`];
  if (k.sugarProp) fields.push(`sugar: "${k.sugarProp}"`);
  tableRows.push(`  ${k.tag}: { ${fields.join(", ")} },`);
}

for (const [name, sugar] of Object.entries(SUGAR)) {
  const of = jsxKinds.find((k) => k.tag === sugar.of);
  if (!of) throw new Error(`gen-types: sugar '${name}' names unknown kind '${sugar.of}'`);
  const presetKeys = Object.keys(sugar.preset).map((key) => `"${key}"`).join(" | ");
  const childrenType = of.hasChildren ? `{ children?: JsxChild }` : `{ children?: never }`;
  intrinsicRows.push(`  ${name}: Omit<${of.optsName}, ${presetKeys}> & ${childrenType};`);
  tableRows.push(
    `  ${name}: { kind: "${sugar.of}", container: ${of.hasChildren}, preset: ${JSON.stringify(sugar.preset)} },`,
  );
}

const jsxImports = jsxKinds.map((k) => k.optsName).sort();
const jsxOut = [
  `// Generated from src/abi/${BUILDERS.schema} by tools/gen-types.ts. Do not edit.`,
  `import type { ${nodeName} } from "./schema";`,
  `import type { ${jsxImports.join(", ")} } from "./ui.gen";`,
  "",
  `export type JsxChild = ${nodeName} | string | number | boolean | null | undefined | JsxChild[];`,
  `export type JsxText = string | number | boolean | null | undefined | JsxText[];`,
  "",
  `export interface IntrinsicElements {`,
  ...intrinsicRows,
  `}`,
  "",
  `export type Intrinsic = {`,
  `  kind: string;`,
  `  container: boolean;`,
  `  preset?: Record<string, unknown>;`,
  `  sugar?: string;`,
  `};`,
  "",
  `export const INTRINSICS: Record<string, Intrinsic> = {`,
  ...tableRows,
  `};`,
  "",
  `export function textNode(text: string): ${nodeName} {`,
  `  return { kind: "${autoTextKind.tag}", props: { ${autoTextKind.sugarProp}: text } };`,
  `}`,
  "",
].join("\n");

writeFileSync(new URL("../src/abi/jsx.gen.ts", import.meta.url), jsxOut);
console.log(`gen-types: ${jsxKinds.length + Object.keys(SUGAR).length} intrinsics -> src/abi/jsx.gen.ts`);
