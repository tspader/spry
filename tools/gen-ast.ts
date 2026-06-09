const inPath = new URL("../src/abi/ui.jtd.json", import.meta.url);
const outPath = new URL("../src/abi/ui.gen.h", import.meta.url);
const enumsHeaderPath = new URL("../src/abi/ui.enums.gen.h", import.meta.url);
const enumsTsPath = new URL("../src/abi/ui.enums.gen.ts", import.meta.url);
const PFX = "spry";

type Schema = any;
const jtd = JSON.parse(await Bun.file(inPath).text());
const defs: Record<string, Schema> = jtd.definitions ?? {};

const defKind: Record<string, "union" | "object"> = {};
for (const [name, d] of Object.entries(defs)) {
  defKind[name] = d.discriminator ? "union" : "object";
}

function hasRequiredFields(schema: Schema): boolean {
  if (schema.ref) return hasRequiredFields(defs[schema.ref]);
  if (schema.discriminator) return true;
  return Object.keys(schema.properties ?? {}).length > 0;
}

const cName = (base: string) => `${PFX}_${base}`;
const cType = (base: string) => `${PFX}_${base}_t`;
const ident = (s: string) => s.replace(/[^A-Za-z0-9]/g, "_");
const enumConst = (base: string, value: string) => `${PFX}_${base}_${value}`.toUpperCase().replace(/[^A-Z0-9_]/g, "_");

const enumTypedefs: Array<{ base: string; consts: string[] }> = [];
const structForward: string[] = [];
const structDefs: string[] = [];
const descForward: string[] = [];
const descDefs: string[] = [];
const enumValues = new Map<string, string>();
const arraysDone = new Set<string>();
const numbersDone = new Set<string>();
const seen = new Set<string>();

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

function ensureNumber(jtdType: string): { cType: string; descRef: string } {
  const n = numTypes[jtdType];
  const name = `spry_${n.cType}_type`;
  if (!numbersDone.has(jtdType)) {
    numbersDone.add(jtdType);
    descForward.push(name);
    descDefs.push(`static const spry_ast_type_t ${name} = { .kind = SPRY_AST_NUMBER, .as.number = { .repr = ${n.repr} } };`);
  }
  return { cType: n.cType, descRef: `&${name}` };
}

function registerEnum(base: string, values: string[]) {
  const key = JSON.stringify(values);
  const prior = enumValues.get(base);
  if (prior !== undefined) {
    if (prior !== key) throw new Error(`gen-ast: enum '${base}' has conflicting value sets`);
    return;
  }
  enumValues.set(base, key);
  enumTypedefs.push({ base, consts: values });
  const namesArr = `${cName(base)}_names`;
  descForward.push(`${cName(base)}_type`);
  descDefs.push(
    `static const c8* const ${namesArr}[] = { ${values.map((v) => `"${v}"`).join(", ")} };\n` +
      `static const spry_ast_type_t ${cName(base)}_type = { .kind = SPRY_AST_ENUM, .as.enom = { .names = ${namesArr}, .count = ${values.length} } };`,
  );
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

function resolveElement(schema: Schema): { base: string; cType: string; descRef: string } {
  if (schema.ref) {
    ensureDef(schema.ref);
    return { base: schema.ref, cType: cType(schema.ref), descRef: `&${cName(schema.ref)}_type` };
  }
  throw new Error(`gen-ast: array elements must be a ref (got ${JSON.stringify(schema)})`);
}

type FieldInfo = { key: string; cFieldType: string; descRef: string; required: boolean; isPtr: boolean; size: string };

function resolveField(parent: string, key: string, sub: Schema, required: boolean): FieldInfo {
  let cTypeName: string;
  let descRef: string;
  let category: "scalar" | "enum" | "object" | "union" | "array";

  if (sub.ref) {
    ensureDef(sub.ref);
    cTypeName = cType(sub.ref);
    descRef = `&${cName(sub.ref)}_type`;
    category = defKind[sub.ref] === "union" ? "union" : "object";
  } else if (sub.type === "string") {
    cTypeName = "sp_str_t"; descRef = "&spry_str_type"; category = "scalar";
  } else if (sub.type === "boolean") {
    cTypeName = "bool"; descRef = "&spry_bool_type"; category = "scalar";
  } else if (sub.type && numTypes[sub.type]) {
    const n = ensureNumber(sub.type);
    cTypeName = n.cType; descRef = n.descRef; category = "scalar";
  } else if (sub.type) {
    throw new Error(`gen-ast: unsupported type '${sub.type}'`);
  } else if (sub.enum) {
    registerEnum(key, sub.enum);
    cTypeName = cType(key); descRef = `&${cName(key)}_type`; category = "enum";
  } else if (sub.elements) {
    const elem = resolveElement(sub.elements);
    cTypeName = `sp_da(${elem.cType})`; descRef = ensureArray(elem); category = "array";
  } else if (sub.discriminator) {
    const base = `${parent}_${key}`;
    emitUnion(base, sub);
    cTypeName = cType(base); descRef = `&${cName(base)}_type`; category = "union";
  } else if (sub.properties || sub.optionalProperties) {
    const base = `${parent}_${key}`;
    emitObject(base, sub);
    cTypeName = cType(base); descRef = `&${cName(base)}_type`; category = "object";
  } else {
    throw new Error(`gen-ast: unsupported schema ${JSON.stringify(sub)}`);
  }

  let isPtr = false;
  let size = "0";
  let cFieldType = cTypeName;
  if ((category === "object" || category === "union") && !required && hasRequiredFields(sub)) {
    isPtr = true;
    cFieldType = `${cTypeName}*`;
    size = `sizeof(${cTypeName})`;
  }
  return { key, cFieldType, descRef, required, isPtr, size };
}

function emitFields(base: string, schema: Schema): FieldInfo[] {
  const out: FieldInfo[] = [];
  for (const [key, sub] of Object.entries(schema.properties ?? {})) {
    out.push(resolveField(base, key, sub, true));
  }
  for (const [key, sub] of Object.entries(schema.optionalProperties ?? {})) {
    out.push(resolveField(base, key, sub, false));
  }
  return out;
}

function emitObject(base: string, schema: Schema) {
  if (seen.has(base)) return;
  seen.add(base);
  structForward.push(base);

  const fields = emitFields(base, schema);

  const members = fields.map((f) => `  ${f.cFieldType} ${f.key};`).join("\n");
  structDefs.push(`typedef struct ${cName(base)} {\n${members}\n} ${cType(base)};`);

  const farr = `${cName(base)}_fields`;
  const lines = fields
    .map(
      (f) =>
        `  { .key = "${f.key}", .offset = offsetof(${cType(base)}, ${f.key}), .type = ${f.descRef}, .required = ${f.required}, .is_ptr = ${f.isPtr}, .size = ${f.size} },`,
    )
    .join("\n");
  descForward.push(`${cName(base)}_type`);
  descDefs.push(
    `static const spry_ast_field_t ${farr}[] = {\n${lines}\n};\n` +
      `static const spry_ast_type_t ${cName(base)}_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = ${farr}, .count = ${fields.length} } };`,
  );
}

function emitUnion(base: string, schema: Schema) {
  if (seen.has(base)) return;
  seen.add(base);
  structForward.push(base);

  const kindBase = `${base}_kind`;
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
    `typedef struct ${cName(base)} {\n  ${cType(kindBase)} kind;\n  union {\n${members.join("\n")}\n  } as;\n} ${cType(base)};`,
  );

  const varr = `${cName(base)}_variants`;
  descForward.push(`${cName(base)}_type`);
  descDefs.push(
    `static const spry_ast_variant_t ${varr}[] = {\n${variantRows.join("\n")}\n};\n` +
      `static const spry_ast_type_t ${cName(base)}_type = { .kind = SPRY_AST_UNION, .as.uni = { .tag_key = "${schema.discriminator}", .tag_offset = offsetof(${cType(base)}, kind), .payload_offset = offsetof(${cType(base)}, as), .variants = ${varr}, .count = ${variants.length} } };`,
  );
}

function ensureDef(name: string) {
  if (seen.has(name)) return;
  const d = defs[name];
  if (!d) throw new Error(`gen-ast: unknown definition '${name}'`);
  if (d.discriminator) emitUnion(name, d);
  else emitObject(name, d);
}

if (!jtd.ref) throw new Error("gen-ast: schema root must be a { ref } to a definition");
ensureDef(jtd.ref);
const rootBase = jtd.ref;

const enumSection = enumTypedefs
  .map((e) => `typedef enum {\n${e.consts.map((c) => `  ${enumConst(e.base, c)},`).join("\n")}\n} ${cType(e.base)};`)
  .join("\n\n");

const enumsHeader = [
  "#ifndef SPRY_UI_ENUMS_GEN_H",
  "#define SPRY_UI_ENUMS_GEN_H",
  "",
  enumSection,
  "",
  "#endif",
  "",
].join("\n");

const enumsTs = [
  "// Generated from src/abi/ui.jtd.json by tools/gen-ast.ts. Do not edit.",
  ...enumTypedefs.flatMap((e) => e.consts.map((c, i) => `export const ${enumConst(e.base, c)} = ${i};`)),
  "",
].join("\n");

const structFwdSection = structForward.map((b) => `typedef struct ${cName(b)} ${cType(b)};`).join("\n");
const scalarSingletons = [
  "static const spry_ast_type_t spry_bool_type = { .kind = SPRY_AST_BOOL };",
  "static const spry_ast_type_t spry_str_type = { .kind = SPRY_AST_STR };",
].join("\n");
const descFwdSection = descForward.map((n) => `static const spry_ast_type_t ${n};`).join("\n");

const out = [
  "#ifndef SPRY_UI_GEN_H",
  "#define SPRY_UI_GEN_H",
  "",
  '#include "spry/ast.h"',
  '#include "abi/ui.enums.gen.h"',
  "",
  structFwdSection,
  "",
  structDefs.join("\n\n"),
  "",
  `typedef ${cType(rootBase)} spry_ast_root_t;`,
  `#define SPRY_AST_ROOT_TYPE ${cName(rootBase)}_type`,
  "",
  scalarSingletons,
  "",
  descFwdSection,
  "",
  descDefs.join("\n\n"),
  "",
  "#endif",
  "",
].join("\n");

await Bun.write(enumsHeaderPath, enumsHeader);
await Bun.write(enumsTsPath, enumsTs);
await Bun.write(outPath, out);
console.log(
  `gen-ast: ${structForward.length} structs, ${enumTypedefs.length} enums ` +
    `-> src/abi/ui.gen.h + ui.enums.gen.h + ui.enums.gen.ts`,
);
