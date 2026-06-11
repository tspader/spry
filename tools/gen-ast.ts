import { readFileSync, writeFileSync } from "node:fs";
import { NODE, INTERACTION, SUGAR, CONTENT, AUTO_TEXT, INVOKE_SUGAR } from "./ui-config";

const PFX = "spry";

const SCHEMAS = [
  { file: "ui.jtd.json", base: "ui", builders: { node: NODE, interaction: INTERACTION } },
  { file: "fault.jtd.json", base: "fault" },
  { file: "endpoints.jtd.json", base: "endpoints" },
];

type Schema = any;

const cName = (base: string) => `${PFX}_${base}`;
const cType = (base: string) => `${PFX}_${base}_t`;
const enumConst = (base: string, value: string) => `${PFX}_${base}_${value}`.toUpperCase().replace(/[^A-Z0-9_]/g, "_");

const numTypes: Record<string, { cType: string; repr: string }> = {
  int8:    { cType: "s8",  repr: "SPRY_NUM_S8"  },
  uint8:   { cType: "u8",  repr: "SPRY_NUM_U8"  },
  int16:   { cType: "s16", repr: "SPRY_NUM_S16" },
  uint16:  { cType: "u16", repr: "SPRY_NUM_U16" },
  int32:   { cType: "s32", repr: "SPRY_NUM_S32" },
  uint32:  { cType: "u32", repr: "SPRY_NUM_U32" },
  float32: { cType: "f32", repr: "SPRY_NUM_F32" },
  float64: { cType: "f64", repr: "SPRY_NUM_F64" },
};

type BuilderCfg = { node: string; interaction: string };

function generate(file: string, base: string, builders?: BuilderCfg) {
  const inPath = new URL(`../src/abi/${file}`, import.meta.url);
  const outPath = new URL(`../src/abi/${base}.gen.h`, import.meta.url);
  const enumsHeaderPath = new URL(`../src/abi/${base}.enums.gen.h`, import.meta.url);
  const enumsTsPath = new URL(`../src/abi/${base}.enums.gen.ts`, import.meta.url);

  const jtd = JSON.parse(readFileSync(inPath, "utf8"));
  const defs: Record<string, Schema> = jtd.definitions ?? {};

  const defKind: Record<string, "union" | "object" | "values"> = {};
  for (const [name, d] of Object.entries<Schema>(defs)) {
    defKind[name] = d.discriminator ? "union" : d.values ? "values" : "object";
  }

  function hasRequiredFields(schema: Schema): boolean {
    if (schema.ref) return hasRequiredFields(defs[schema.ref]);
    if (schema.discriminator) return true;
    if (schema.values) return false;
    return Object.keys(schema.properties ?? {}).length > 0;
  }

  const enumTypedefs: Array<{ base: string; consts: string[] }> = [];
  const structForward: string[] = [];
  const structDefs: string[] = [];
  const descForward: string[] = [];
  const descDefs: string[] = [];
  const enumValues = new Map<string, string>();
  const arraysDone = new Set<string>();
  const valuesDone = new Set<string>();
  const seen = new Set<string>();

  function registerEnum(key: string, values: string[]) {
    const sig = JSON.stringify(values);
    const prior = enumValues.get(key);
    if (prior !== undefined) {
      if (prior !== sig) throw new Error(`gen-ast: enum '${key}' has conflicting value sets`);
      return;
    }
    enumValues.set(key, sig);
    enumTypedefs.push({ base: key, consts: values });
    const namesArr = `${cName(key)}_names`;
    descForward.push(`${cName(key)}_type`);
    descDefs.push(
      `static const c8* const ${namesArr}[] = { ${values.map((v) => `"${v}"`).join(", ")} };\n` +
        `static const spry_ast_type_t ${cName(key)}_type = { .kind = SPRY_AST_ENUM, .as.enom = { .names = ${namesArr}, .count = ${values.length} } };`,
    );
  }

  type Resolved = { cType: string; descRef: string; category: "scalar" | "enum" | "object" | "union" | "array" | "values" };

  function resolveType(parent: string, key: string, sub: Schema): Resolved {
    if (sub.ref) {
      ensureDef(sub.ref);
      const category = defKind[sub.ref] === "union" ? "union" : defKind[sub.ref] === "values" ? "values" : "object";
      return { cType: cType(sub.ref), descRef: `&${cName(sub.ref)}_type`, category };
    }
    if (sub.type === "string") return { cType: "sp_str_t", descRef: "&spry_str_type", category: "scalar" };
    if (sub.type === "boolean") return { cType: "bool", descRef: "&spry_bool_type", category: "scalar" };
    if (sub.type && numTypes[sub.type]) {
      const n = numTypes[sub.type]!;
      return { cType: n.cType, descRef: `&spry_${n.cType}_type`, category: "scalar" };
    }
    if (sub.type) throw new Error(`gen-ast: unsupported type '${sub.type}'`);
    if (sub.enum) {
      registerEnum(key, sub.enum);
      return { cType: cType(key), descRef: `&${cName(key)}_type`, category: "enum" };
    }
    if (sub.elements) {
      const elem = resolveElement(sub.elements);
      return { cType: `sp_da(${elem.cType})`, descRef: ensureArray(elem), category: "array" };
    }
    if (sub.values) {
      return emitValues(`${parent}_${key}`, sub.values);
    }
    if (sub.discriminator) {
      const vbase = `${parent}_${key}`;
      emitUnion(vbase, sub);
      return { cType: cType(vbase), descRef: `&${cName(vbase)}_type`, category: "union" };
    }
    if (sub.properties || sub.optionalProperties) {
      const obase = `${parent}_${key}`;
      emitObject(obase, sub);
      return { cType: cType(obase), descRef: `&${cName(obase)}_type`, category: "object" };
    }
    throw new Error(`gen-ast: unsupported schema ${JSON.stringify(sub)}`);
  }

  function resolveElement(schema: Schema): { base: string; cType: string; descRef: string } {
    if (schema.ref) {
      ensureDef(schema.ref);
      return { base: schema.ref, cType: cType(schema.ref), descRef: `&${cName(schema.ref)}_type` };
    }
    throw new Error(`gen-ast: array elements must be a ref (got ${JSON.stringify(schema)})`);
  }

  function ensureArray(elem: { base: string; cType: string; descRef: string }): string {
    const name = `${cName(elem.base)}_array_type`;
    if (!arraysDone.has(elem.base)) {
      arraysDone.add(elem.base);
      descForward.push(name);
      descDefs.push(
        `static const spry_ast_type_t ${name} = { .kind = SPRY_AST_ARRAY, .as.array = { .element = ${elem.descRef}, .stride = sizeof(${elem.cType}) } };`,
      );
    }
    return `&${name}`;
  }

  function emitValues(vbase: string, valueSchema: Schema): Resolved {
    const entry = `${vbase}_entry`;
    if (!valuesDone.has(vbase)) {
      valuesDone.add(vbase);
      const value = resolveType(vbase, "value", valueSchema);
      structForward.push(entry);
      structDefs.push(`typedef struct ${cName(entry)} {\n  sp_str_t key;\n  ${value.cType} value;\n} ${cType(entry)};`);
      descForward.push(`${cName(vbase)}_type`);
      descDefs.push(
        `static const spry_ast_type_t ${cName(vbase)}_type = { .kind = SPRY_AST_VALUES, .as.values = { .value = ${value.descRef}, .stride = sizeof(${cType(entry)}), .key_offset = offsetof(${cType(entry)}, key), .value_offset = offsetof(${cType(entry)}, value) } };`,
      );
    }
    return { cType: `sp_da(${cType(entry)})`, descRef: `&${cName(vbase)}_type`, category: "values" };
  }

  type FieldInfo = { key: string; cFieldType: string; descRef: string; required: boolean; isPtr: boolean; size: string };

  function resolveField(parent: string, key: string, sub: Schema, required: boolean): FieldInfo {
    const r = resolveType(parent, key, sub);

    let isPtr = false;
    let size = "0";
    let cFieldType = r.cType;
    if ((r.category === "object" || r.category === "union") && !required && hasRequiredFields(sub)) {
      isPtr = true;
      cFieldType = `${r.cType}*`;
      size = `sizeof(${r.cType})`;
    }
    return { key, cFieldType, descRef: r.descRef, required, isPtr, size };
  }

  function emitFields(obase: string, schema: Schema): FieldInfo[] {
    const out: FieldInfo[] = [];
    for (const [key, sub] of Object.entries<Schema>(schema.properties ?? {})) {
      out.push(resolveField(obase, key, sub, true));
    }
    for (const [key, sub] of Object.entries<Schema>(schema.optionalProperties ?? {})) {
      out.push(resolveField(obase, key, sub, false));
    }
    return out;
  }

  function emitObject(obase: string, schema: Schema) {
    if (seen.has(obase)) return;
    seen.add(obase);
    structForward.push(obase);

    const fields = emitFields(obase, schema);

    const members = fields.map((f) => `  ${f.cFieldType} ${f.key};`).join("\n");
    structDefs.push(`typedef struct ${cName(obase)} {\n${members}\n} ${cType(obase)};`);

    const farr = `${cName(obase)}_fields`;
    const lines = fields
      .map(
        (f) =>
          `  { .key = "${f.key}", .offset = offsetof(${cType(obase)}, ${f.key}), .type = ${f.descRef}, .required = ${f.required}, .is_ptr = ${f.isPtr}, .size = ${f.size} },`,
      )
      .join("\n");
    descForward.push(`${cName(obase)}_type`);
    descDefs.push(
      `static const spry_ast_field_t ${farr}[] = {\n${lines}\n};\n` +
        `static const spry_ast_type_t ${cName(obase)}_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = ${farr}, .count = ${fields.length} } };`,
    );
  }

  function emitUnion(ubase: string, schema: Schema) {
    if (seen.has(ubase)) return;
    seen.add(ubase);
    structForward.push(ubase);

    const kindBase = `${ubase}_kind`;
    const variants = Object.keys(schema.mapping ?? {});
    enumTypedefs.push({ base: kindBase, consts: variants });

    const members: string[] = [];
    const variantRows: string[] = [];
    for (const vn of variants) {
      emitObject(vn, schema.mapping[vn]);
      members.push(`    ${cType(vn)} ${vn};`);
      variantRows.push(`  { .tag = "${vn}", .value = ${enumConst(kindBase, vn)}, .type = &${cName(vn)}_type },`);
    }

    structDefs.push(
      `typedef struct ${cName(ubase)} {\n  ${cType(kindBase)} kind;\n  union {\n${members.join("\n")}\n  } as;\n} ${cType(ubase)};`,
    );

    const varr = `${cName(ubase)}_variants`;
    descForward.push(`${cName(ubase)}_type`);
    descDefs.push(
      `static const spry_ast_variant_t ${varr}[] = {\n${variantRows.join("\n")}\n};\n` +
        `static const spry_ast_type_t ${cName(ubase)}_type = { .kind = SPRY_AST_UNION, .as.uni = { .tag_key = "${schema.discriminator}", .tag_offset = offsetof(${cType(ubase)}, kind), .payload_offset = offsetof(${cType(ubase)}, as), .variants = ${varr}, .count = ${variants.length} } };`,
    );
  }

  function emitValuesDef(vbase: string, schema: Schema) {
    if (seen.has(vbase)) return;
    seen.add(vbase);
    const r = emitValues(vbase, schema.values);
    structDefs.push(`typedef ${r.cType} ${cType(vbase)};`);
  }

  function ensureDef(name: string) {
    if (seen.has(name)) return;
    const d = defs[name];
    if (!d) throw new Error(`gen-ast: unknown definition '${name}'`);
    if (d.discriminator) emitUnion(name, d);
    else if (d.values) emitValuesDef(name, d);
    else emitObject(name, d);
  }

  if (!jtd.ref) throw new Error("gen-ast: schema root must be a { ref } to a definition");
  ensureDef(jtd.ref);

  const enumSection = enumTypedefs
    .map((e) => `typedef enum {\n${e.consts.map((c) => `  ${enumConst(e.base, c)},`).join("\n")}\n} ${cType(e.base)};`)
    .join("\n\n");

  const guard = base.toUpperCase().replace(/[^A-Z0-9]/g, "_");

  const enumsHeader = [
    `#ifndef SPRY_${guard}_ENUMS_GEN_H`,
    `#define SPRY_${guard}_ENUMS_GEN_H`,
    "",
    enumSection,
    "",
    "#endif",
    "",
  ].join("\n");

  const enumsTs = [
    `// Generated from src/abi/${file} by tools/gen-ast.ts. Do not edit.`,
    ...enumTypedefs.flatMap((e) => e.consts.map((c, i) => `export const ${enumConst(e.base, c)} = ${i};`)),
    "",
  ].join("\n");

  const structFwdSection = structForward.map((b) => `typedef struct ${cName(b)} ${cType(b)};`).join("\n");
  const descFwdSection = descForward.map((n) => `static const spry_ast_type_t ${n};`).join("\n");

  const out = [
    `#ifndef SPRY_${guard}_GEN_H`,
    `#define SPRY_${guard}_GEN_H`,
    "",
    '#include "spry/ast.h"',
    `#include "abi/${base}.enums.gen.h"`,
    "",
    structFwdSection,
    "",
    structDefs.join("\n\n"),
    "",
    descFwdSection,
    "",
    descDefs.join("\n\n"),
    "",
    "#endif",
    "",
  ].join("\n");

  writeFileSync(outPath, out);
  writeFileSync(enumsHeaderPath, enumsHeader);
  writeFileSync(enumsTsPath, enumsTs);
  console.log(`gen-ast: ${structDefs.length} structs, ${enumTypedefs.length} enums -> src/abi/${base}.gen.h`);

  if (builders) emitBuilders(base, defs, builders);
}

function emitBuilders(base: string, defs: Record<string, Schema>, cfg: BuilderCfg) {
  const node = defs[cfg.node];
  const interaction = defs[cfg.interaction];
  if (!node?.discriminator || !interaction?.discriminator) {
    throw new Error("gen-ast: builders config must name union definitions");
  }

  const guard = base.toUpperCase().replace(/[^A-Z0-9]/g, "_");
  const fns: string[] = [];
  const decls: string[] = [];
  const macros: string[] = [];

  function declFieldType(key: string, sub: Schema): string {
    if (sub.type === "string") return "sp_str_t";
    if (sub.type === "boolean") return "bool";
    if (sub.type && numTypes[sub.type]) return numTypes[sub.type]!.cType;
    if (sub.enum) return cType(key);
    throw new Error(`gen-ast: unsupported decl field '${key}'`);
  }

  function variantProps(tag: string, variant: Schema): Schema {
    const props = variant.properties?.props ?? variant.optionalProperties?.props;
    if (!props) throw new Error(`gen-ast: builder variant '${tag}' has no props`);
    return props;
  }

  function propFieldEntries(props: Schema): Array<[string, Schema]> {
    return [...Object.entries<Schema>(props.properties ?? {}), ...Object.entries<Schema>(props.optionalProperties ?? {})];
  }

  for (const [tag, variant] of Object.entries<Schema>(node.mapping)) {
    const props = variantProps(tag, variant);
    const propFields = propFieldEntries(props);
    const hasOn = "on" in (variant.properties ?? {}) || "on" in (variant.optionalProperties ?? {});
    for (const [key] of propFields) {
      if (key === "id" || key === "on") throw new Error(`gen-ast: prop '${key}' of '${tag}' collides with a node field`);
    }

    const declName = cType(`${tag}_decl`);
    const fields = [`  const void* spry_ui__designated;`];
    fields.push(...propFields.map(([key, sub]) => `  ${declFieldType(key, sub)} ${key};`));
    fields.push(`  sp_str_t id;`);
    if (hasOn) fields.push(`  const ${cType(cfg.interaction)}* on;`);
    decls.push(`typedef struct ${cName(`${tag}_decl`)} {\n${fields.join("\n")}\n} ${declName};`);

    const open = [`static inline u32 ${cName(`ui_open_${tag}`)}(spry_ui_t* ui, ${declName} decl) {`];
    open.push(`  ${cType(cfg.node)} node = sp_zero_s(${cType(cfg.node)});`);
    open.push(`  node.kind = ${enumConst(`${cfg.node}_kind`, tag)};`);
    open.push(`  node.as.${tag}.id = decl.id;`);
    for (const [key] of propFields) open.push(`  node.as.${tag}.props.${key} = decl.${key};`);
    open.push(`  u32 opened = spry_ui_enter(ui, node);`);
    if (hasOn) open.push(`  if (decl.on) spry_ui_set_on(ui, opened, *decl.on);`);
    open.push(`  return opened;`);
    open.push(`}`);
    fns.push(open.join("\n"));

    const content = CONTENT[tag];
    if (content) {
      const sub = (props.properties ?? {})[content] ?? (props.optionalProperties ?? {})[content];
      if (sub?.type !== "string") throw new Error(`gen-ast: content prop '${content}' of '${tag}' is not a string`);
      macros.push(
        `#define SPRY_${tag.toUpperCase()}(spry__content, ...) SPRY_UI_ELEMENT(${cName(`ui_open_${tag}`)}(spry_ui__ctx, (${declName}){ .${content} = spry_str(spry__content), __VA_ARGS__ }))`,
      );
    } else {
      macros.push(
        `#define SPRY_${tag.toUpperCase()}(...) SPRY_UI_ELEMENT(${cName(`ui_open_${tag}`)}(spry_ui__ctx, (${declName}){ __VA_ARGS__ }))`,
      );
    }
  }

  for (const [name, sugar] of Object.entries(SUGAR)) {
    const variant = node.mapping[sugar.of];
    if (!variant) throw new Error(`gen-ast: sugar '${name}' names unknown variant '${sugar.of}'`);
    const props = variantProps(sugar.of, variant);
    const preset = Object.entries(sugar.preset).map(([key, value]) => {
      const sub = (props.properties ?? {})[key] ?? (props.optionalProperties ?? {})[key];
      if (!sub?.enum?.includes(value)) throw new Error(`gen-ast: sugar '${name}' preset '${key}=${value}' is not an enum value`);
      return `.${key} = ${enumConst(key, value)}`;
    });
    macros.push(
      `#define SPRY_${name.toUpperCase()}(...) SPRY_UI_ELEMENT(${cName(`ui_open_${sugar.of}`)}(spry_ui__ctx, (${cType(`${sugar.of}_decl`)}){ ${preset.join(", ")}, __VA_ARGS__ }))`,
    );
  }

  {
    const variant = node.mapping[AUTO_TEXT.kind];
    if (!variant) throw new Error(`gen-ast: autoText kind '${AUTO_TEXT.kind}' is not a variant`);
    macros.push(
      `#define SPRY_${AUTO_TEXT.kind.toUpperCase()}F(...) SPRY_UI_ELEMENT(${cName(`ui_open_${AUTO_TEXT.kind}`)}(spry_ui__ctx, (${cType(`${AUTO_TEXT.kind}_decl`)}){ .${AUTO_TEXT.prop} = spry_ui_fmt(spry_ui__ctx, __VA_ARGS__) }))`,
    );
  }

  for (const tag of Object.keys(interaction.mapping)) {
    macros.push(
      `#define SPRY_${tag.toUpperCase()}(...) (&(const ${cType(cfg.interaction)}){ .kind = ${enumConst(`${cfg.interaction}_kind`, tag)}, .as.${tag} = { __VA_ARGS__ } })`,
    );
  }

  for (const [name, sugar] of Object.entries(INVOKE_SUGAR)) {
    const variant = interaction.mapping[sugar.of];
    if (!variant) throw new Error(`gen-ast: invoke sugar '${name}' names unknown variant '${sugar.of}'`);
    const fields = { ...variant.properties, ...variant.optionalProperties } as Record<string, Schema>;
    const params: string[] = [];
    const inits: string[] = [];
    for (const arg of sugar.args) {
      const sub = fields[arg];
      if (!sub) throw new Error(`gen-ast: invoke sugar '${name}' arg '${arg}' is not a field of '${sugar.of}'`);
      if (sub.type !== "string") continue;
      params.push(`spry__${arg}`);
      inits.push(`.${arg} = spry_str(spry__${arg})`);
    }
    for (const [key, value] of Object.entries(sugar.preset)) {
      const sub = fields[key];
      if (!sub?.enum?.includes(value)) throw new Error(`gen-ast: invoke sugar '${name}' preset '${key}=${value}' is not an enum value`);
      inits.push(`.${key} = ${enumConst(key, value)}`);
    }
    macros.push(
      `#define SPRY_${name.toUpperCase()}(${params.join(", ")}) SPRY_${sugar.of.toUpperCase()}(${inits.join(", ")})`,
    );
  }

  const out = [
    `#ifndef SPRY_${guard}_BUILDERS_GEN_H`,
    `#define SPRY_${guard}_BUILDERS_GEN_H`,
    "",
    `#include "abi/${base}.gen.h"`,
    "",
    "typedef struct spry_ui spry_ui_t;",
    "",
    `u32 spry_ui_enter(spry_ui_t* ui, ${cType(cfg.node)} node);`,
    `void spry_ui_set_on(spry_ui_t* ui, u32 node, ${cType(cfg.interaction)} on);`,
    "",
    decls.join("\n\n"),
    "",
    fns.join("\n\n"),
    "",
    macros.join("\n\n"),
    "",
    "#endif",
    "",
  ].join("\n");

  writeFileSync(new URL(`../src/abi/${base}.builders.gen.h`, import.meta.url), out);
  console.log(`gen-ast: ${fns.length} builders, ${macros.length} macros -> src/abi/${base}.builders.gen.h`);
}

for (const s of SCHEMAS) generate(s.file, s.base, s.builders);
