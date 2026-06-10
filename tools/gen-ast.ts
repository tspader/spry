import { readFileSync, writeFileSync } from "node:fs";

const PFX = "spry";

const SCHEMAS = [
  { file: "ui.jtd.json", base: "ui" },
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

function generate(file: string, base: string) {
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
  const sharedDefs: string[] = [];
  const enumValues = new Map<string, string>();
  const arraysDone = new Set<string>();
  const valuesDone = new Set<string>();
  const numbersDone = new Set<string>();
  const seen = new Set<string>();

  function shared(guard: string, def: string) {
    sharedDefs.push(`#ifndef ${guard}\n#define ${guard}\n${def}\n#endif`);
  }

  function ensureNumber(jtdType: string): { cType: string; descRef: string } {
    const n = numTypes[jtdType]!;
    const name = `spry_${n.cType}_type`;
    if (!numbersDone.has(jtdType)) {
      numbersDone.add(jtdType);
      shared(`SPRY_GEN_SCALAR_${n.cType.toUpperCase()}`,
        `static const spry_ast_type_t ${name} = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = ${n.repr} } };`);
    }
    return { cType: n.cType, descRef: `&${name}` };
  }

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
      const n = ensureNumber(sub.type);
      return { cType: n.cType, descRef: n.descRef, category: "scalar" };
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
  shared("SPRY_GEN_SCALAR_BOOL", "static const spry_ast_type_t spry_bool_type = { .kind = SPRY_AST_BOOL };");
  shared("SPRY_GEN_SCALAR_STR", "static const spry_ast_type_t spry_str_type = { .kind = SPRY_AST_STR };");
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
    sharedDefs.join("\n\n"),
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
}

for (const s of SCHEMAS) generate(s.file, s.base);
