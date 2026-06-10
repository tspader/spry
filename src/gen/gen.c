#include "sp.h"
#include "spry/core.h"
#include "spry/gen.h"

typedef struct {
  const c8* c_type;
  const c8* desc;
  const c8* ts;
} gen_type_info_t;

static const gen_type_info_t GEN_TYPES[] = {
  [SPRY_TYPE_STRING]  = { "sp_str_t", "spry_str_type",  "string" },
  [SPRY_TYPE_BOOLEAN] = { "bool",     "spry_bool_type", "boolean" },
  [SPRY_TYPE_INT8]    = { "s8",       "spry_s8_type",   "number" },
  [SPRY_TYPE_UINT8]   = { "u8",       "spry_u8_type",   "number" },
  [SPRY_TYPE_INT16]   = { "s16",      "spry_s16_type",  "number" },
  [SPRY_TYPE_UINT16]  = { "u16",      "spry_u16_type",  "number" },
  [SPRY_TYPE_INT32]   = { "s32",      "spry_s32_type",  "number" },
  [SPRY_TYPE_UINT32]  = { "u32",      "spry_u32_type",  "number" },
  [SPRY_TYPE_FLOAT32] = { "f32",      "spry_f32_type",  "number" },
  [SPRY_TYPE_FLOAT64] = { "f64",      "spry_f64_type",  "number" },
};

static const c8* GEN_RESERVED[] = {
  "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else",
  "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register",
  "restrict", "return", "short", "signed", "sizeof", "static", "struct", "switch", "typedef",
  "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool",
  "_Complex", "_Generic", "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local",
  "bool", "true", "false", "offsetof",
};

typedef struct {
  sp_str_t key;
  spry_type_t type;
  bool required;
} gen_field_t;

typedef struct {
  sp_str_t name;
  sp_str_t sym;
  sp_str_t pascal;
  sp_da(gen_field_t) fields;
} gen_endpoint_t;

typedef struct {
  sp_str_t prefix;
  sp_str_t guard;
  sp_da(gen_endpoint_t) endpoints;
} gen_model_t;

typedef struct {
  sp_io_writer_t* io;
  sp_err_t err;
} gen_w_t;

static void gen_emit(gen_w_t* w, const c8* fmt, ...) {
  if (w->err) return;
  va_list args;
  va_start(args, fmt);
  w->err = sp_fmt_io_v(w->io, sp_cstr_as_str(fmt), args);
  va_end(args);
}

static bool gen_is_ident(sp_str_t s) {
  if (!s.len) return false;
  sp_for(i, s.len) {
    c8 c = s.data[i];
    bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    bool digit = c >= '0' && c <= '9';
    if (!alpha && !(i && digit)) return false;
  }
  return true;
}

static bool gen_is_reserved(sp_str_t s) {
  sp_carr_for(GEN_RESERVED, i) {
    if (sp_str_equal_cstr(s, GEN_RESERVED[i])) return true;
  }
  return false;
}

static sp_str_t gen_upper(sp_mem_t mem, sp_str_t name) {
  sp_str_t out = sp_str_copy(mem, name);
  sp_for(i, out.len) {
    c8 c = out.data[i];
    if (c >= 'a' && c <= 'z') ((c8*)out.data)[i] = (c8)(c - 'a' + 'A');
  }
  return out;
}

static sp_str_t gen_pascal(sp_mem_t mem, sp_str_t name) {
  sp_da(c8) out = sp_da_new(mem, c8);
  bool upper = true;
  sp_for(i, name.len) {
    c8 c = name.data[i];
    if (c == '_') {
      upper = true;
      continue;
    }
    if (upper && c >= 'a' && c <= 'z') c = (c8)(c - 'a' + 'A');
    upper = false;
    sp_da_push(out, c);
  }
  return sp_str(out, (u32)sp_da_size(out));
}

#define gen_try(expr) \
  do { \
    spry_gen_err_t _gen_err = (expr); \
    if (_gen_err.code) return _gen_err; \
  } while (0)

static spry_gen_err_t gen_field_add(gen_endpoint_t* ep, sp_str_t key, spry_type_t type, bool required) {
  if (!gen_is_ident(key)) return (spry_gen_err_t){ .code = SPRY_ERR_GEN_ARG_INVALID, .as.arg = { .endpoint = ep->name, .name = key } };
  if (gen_is_reserved(key)) return (spry_gen_err_t){ .code = SPRY_ERR_GEN_ARG_RESERVED, .as.arg = { .endpoint = ep->name, .name = key } };
  if (type >= sp_carr_len(GEN_TYPES) || !GEN_TYPES[type].c_type) return (spry_gen_err_t){ .code = SPRY_ERR_GEN_ARG_TYPE, .as.arg = { .endpoint = ep->name, .name = key } };
  sp_da_for(ep->fields, i) {
    if (sp_str_equal(ep->fields[i].key, key)) return (spry_gen_err_t){ .code = SPRY_ERR_GEN_ARG_DUP, .as.arg = { .endpoint = ep->name, .name = key } };
  }
  gen_field_t field = { .key = key, .type = type, .required = required };
  sp_da_push(ep->fields, field);
  return sp_zero_s(spry_gen_err_t);
}

static spry_gen_err_t gen_model_build(sp_mem_t mem, spry_gen_opts_t opts, gen_model_t* out) {
  if (sp_str_empty(opts.prefix)) return (spry_gen_err_t){ .code = SPRY_ERR_GEN_PREFIX_MISSING };
  if (!gen_is_ident(opts.prefix)) return (spry_gen_err_t){ .code = SPRY_ERR_GEN_PREFIX_INVALID, .as.prefix = { .name = opts.prefix } };

  spry_endpoints_t endpoints;
  spry_issue_t issue = sp_zero_s(spry_issue_t);
  spry_err_t parsed = spry_endpoints_parse(mem, opts.endpoints, &endpoints, &issue);
  if (parsed) return (spry_gen_err_t){ .code = parsed, .as.parse = issue };

  out->prefix = opts.prefix;
  out->guard = gen_upper(mem, opts.prefix);
  out->endpoints = sp_da_new(mem, gen_endpoint_t);

  sp_da_for(endpoints, i) {
    sp_str_t name = endpoints[i].key;
    if (!gen_is_ident(name)) return (spry_gen_err_t){ .code = SPRY_ERR_GEN_ENDPOINT_INVALID, .as.endpoint = { .name = name } };
    sp_da_for(out->endpoints, j) {
      if (sp_str_equal(out->endpoints[j].name, name)) return (spry_gen_err_t){ .code = SPRY_ERR_GEN_ENDPOINT_DUP, .as.endpoint = { .name = name } };
    }

    gen_endpoint_t ep = {
      .name = name,
      .sym = sp_fmt(mem, "{}_{}", sp_fmt_str(opts.prefix), sp_fmt_str(name)).value,
      .pascal = gen_pascal(mem, name),
      .fields = sp_da_new(mem, gen_field_t),
    };

    const spry_endpoint_args_t* args = &endpoints[i].value.args;
    sp_da_for(args->properties, j) {
      gen_try(gen_field_add(&ep, args->properties[j].key, args->properties[j].value.type, true));
    }
    sp_da_for(args->optionalProperties, j) {
      gen_try(gen_field_add(&ep, args->optionalProperties[j].key, args->optionalProperties[j].value.type, false));
    }

    sp_da_push(out->endpoints, ep);
  }
  return sp_zero_s(spry_gen_err_t);
}

static spry_gen_err_t gen_finish(gen_w_t* w) {
  if (!w->err) return sp_zero_s(spry_gen_err_t);
  return (spry_gen_err_t){ .code = SPRY_ERR_GEN_WRITE, .as.write = w->err };
}

sp_str_t spry_gen_err_str(sp_mem_t mem, const spry_gen_err_t* error) {
  switch (error->code) {
    case SPRY_ERR_GEN_PREFIX_MISSING:   return sp_str_lit("prefix is required");
    case SPRY_ERR_GEN_PREFIX_INVALID:   return sp_fmt(mem, "prefix '{}' is not a valid identifier", sp_fmt_str(error->as.prefix.name)).value;
    case SPRY_ERR_GEN_ENDPOINT_INVALID: return sp_fmt(mem, "endpoint '{}' is not a valid identifier", sp_fmt_str(error->as.endpoint.name)).value;
    case SPRY_ERR_GEN_ENDPOINT_DUP:     return sp_fmt(mem, "duplicate endpoint '{}'", sp_fmt_str(error->as.endpoint.name)).value;
    case SPRY_ERR_GEN_ARG_INVALID:      return sp_fmt(mem, "endpoint '{}': argument '{}' is not a valid identifier", sp_fmt_str(error->as.arg.endpoint), sp_fmt_str(error->as.arg.name)).value;
    case SPRY_ERR_GEN_ARG_RESERVED:     return sp_fmt(mem, "endpoint '{}': argument '{}' is a reserved word", sp_fmt_str(error->as.arg.endpoint), sp_fmt_str(error->as.arg.name)).value;
    case SPRY_ERR_GEN_ARG_DUP:          return sp_fmt(mem, "endpoint '{}': duplicate argument '{}'", sp_fmt_str(error->as.arg.endpoint), sp_fmt_str(error->as.arg.name)).value;
    case SPRY_ERR_GEN_ARG_TYPE:         return sp_fmt(mem, "endpoint '{}': argument '{}' has an unsupported type", sp_fmt_str(error->as.arg.endpoint), sp_fmt_str(error->as.arg.name)).value;
    case SPRY_ERR_GEN_WRITE:            return sp_fmt(mem, "write failed ({})", sp_fmt_int(error->as.write)).value;
    default:                            return spry_issue_str(mem, &error->as.parse);
  }
}

spry_gen_err_t spry_gen_c(sp_mem_t mem, spry_gen_opts_t opts, sp_io_writer_t* io) {
  gen_model_t model;
  gen_try(gen_model_build(mem, opts, &model));

  gen_w_t w = { .io = io };

  gen_emit(&w, "#ifndef {}_ENDPOINTS_GEN_H\n", sp_fmt_str(model.guard));
  gen_emit(&w, "#define {}_ENDPOINTS_GEN_H\n", sp_fmt_str(model.guard));
  gen_emit(&w, "\n");
  gen_emit(&w, "#include \"spry/core.h\"\n");
  gen_emit(&w, "#include \"spry/rpc.h\"\n");

  sp_da_for(model.endpoints, i) {
    const gen_endpoint_t* ep = &model.endpoints[i];
    sp_str_t sym = ep->sym;
    bool has_args = sp_da_size(ep->fields) > 0;

    gen_emit(&w, "\n");

    if (has_args) {
      gen_emit(&w, "typedef struct {{\n");
      sp_da_for(ep->fields, j) {
        const gen_field_t* field = &ep->fields[j];
        gen_emit(&w, "  {} {};\n", sp_fmt_cstr(GEN_TYPES[field->type].c_type), sp_fmt_str(field->key));
      }
      gen_emit(&w, "}} {}_args_t;\n", sp_fmt_str(sym));
      gen_emit(&w, "\n");

      gen_emit(&w, "static const spry_ast_field_t {}_args_fields[] = {{\n", sp_fmt_str(sym));
      sp_da_for(ep->fields, j) {
        const gen_field_t* field = &ep->fields[j];
        gen_emit(&w, "  {{ .key = \"{}\", .offset = offsetof({}_args_t, {}), .type = &{}, .required = {}, .is_ptr = false, .size = 0 }},\n",
          sp_fmt_str(field->key), sp_fmt_str(sym), sp_fmt_str(field->key),
          sp_fmt_cstr(GEN_TYPES[field->type].desc), sp_fmt_cstr(field->required ? "true" : "false"));
      }
      gen_emit(&w, "}};\n");
      gen_emit(&w, "static const spry_ast_type_t {}_args_type = {{ .kind = SPRY_AST_OBJECT, .as.object = {{ .fields = {}_args_fields, .count = {} }} }};\n",
        sp_fmt_str(sym), sp_fmt_str(sym), sp_fmt_uint(sp_da_size(ep->fields)));
      gen_emit(&w, "\n");
      gen_emit(&w, "typedef spry_reply_t (*{}_fn_t)(void* ctx, const {}_args_t* args);\n", sp_fmt_str(sym), sp_fmt_str(sym));
    } else {
      gen_emit(&w, "typedef spry_reply_t (*{}_fn_t)(void* ctx);\n", sp_fmt_str(sym));
    }

    gen_emit(&w, "\n");
    gen_emit(&w, "static inline spry_reply_t {}_thunk(void* ctx, const spry_args_t* args) {{\n", sp_fmt_str(sym));
    gen_emit(&w, "  spry_binding_t* binding = ctx;\n");
    if (has_args) {
      gen_emit(&w, "  {}_args_t out = sp_zero_s({}_args_t);\n", sp_fmt_str(sym), sp_fmt_str(sym));
      gen_emit(&w, "  spry_reply_t fault;\n");
      gen_emit(&w, "  if (!spry_rpc_parse(binding, args, &{}_args_type, &out, &fault)) return fault;\n", sp_fmt_str(sym));
      gen_emit(&w, "  return (({}_fn_t)binding->fn)(binding->ctx, &out);\n", sp_fmt_str(sym));
    } else {
      gen_emit(&w, "  (void)args;\n");
      gen_emit(&w, "  return (({}_fn_t)binding->fn)(binding->ctx);\n", sp_fmt_str(sym));
    }
    gen_emit(&w, "}}\n");
    gen_emit(&w, "\n");

    gen_emit(&w, "static inline void {}_register_{}(spry_rpc_t* rpc, {}_fn_t fn, void* ctx) {{\n",
      sp_fmt_str(model.prefix), sp_fmt_str(ep->name), sp_fmt_str(sym));
    gen_emit(&w, "  spry_rpc_bind(rpc, sp_str_lit(\"{}\"), (spry_handler_any_t)fn, ctx, {}_thunk);\n", sp_fmt_str(ep->name), sp_fmt_str(sym));
    gen_emit(&w, "}}\n");
  }

  gen_emit(&w, "\n");
  gen_emit(&w, "#endif\n");

  return gen_finish(&w);
}

spry_gen_err_t spry_gen_ts(sp_mem_t mem, spry_gen_opts_t opts, sp_io_writer_t* io) {
  gen_model_t model;
  gen_try(gen_model_build(mem, opts, &model));

  gen_w_t w = { .io = io };
  gen_emit(&w, "// Generated by spry. Do not edit.\n");

  sp_da_for(model.endpoints, i) {
    const gen_endpoint_t* ep = &model.endpoints[i];
    if (!sp_da_size(ep->fields)) continue;
    gen_emit(&w, "\n");
    gen_emit(&w, "export type {}Args = {{\n", sp_fmt_str(ep->pascal));
    sp_da_for(ep->fields, j) {
      const gen_field_t* field = &ep->fields[j];
      gen_emit(&w, "  {}{}: {};\n", sp_fmt_str(field->key), sp_fmt_cstr(field->required ? "" : "?"), sp_fmt_cstr(GEN_TYPES[field->type].ts));
    }
    gen_emit(&w, "}};\n");
  }

  gen_emit(&w, "\n");
  gen_emit(&w, "export type Handlers = {{\n");
  sp_da_for(model.endpoints, i) {
    const gen_endpoint_t* ep = &model.endpoints[i];
    if (sp_da_size(ep->fields)) {
      gen_emit(&w, "  {}(args: {}Args): unknown | Promise<unknown>;\n", sp_fmt_str(ep->name), sp_fmt_str(ep->pascal));
    } else {
      gen_emit(&w, "  {}(): unknown | Promise<unknown>;\n", sp_fmt_str(ep->name));
    }
  }
  gen_emit(&w, "}};\n");

  return gen_finish(&w);
}
