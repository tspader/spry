#ifndef SPRY_ENDPOINTS_GEN_H
#define SPRY_ENDPOINTS_GEN_H

#include "spry/ast.h"
#include "abi/endpoints.enums.gen.h"

typedef struct spry_endpoint spry_endpoint_t;
typedef struct spry_endpoint_args spry_endpoint_args_t;
typedef struct spry_endpoint_arg spry_endpoint_arg_t;
typedef struct spry_endpoint_args_properties_entry spry_endpoint_args_properties_entry_t;
typedef struct spry_endpoint_args_optionalProperties_entry spry_endpoint_args_optionalProperties_entry_t;
typedef struct spry_endpoints_entry spry_endpoints_entry_t;

typedef struct spry_endpoint_arg {
  spry_type_t type;
} spry_endpoint_arg_t;

typedef struct spry_endpoint_args_properties_entry {
  sp_str_t key;
  spry_endpoint_arg_t value;
} spry_endpoint_args_properties_entry_t;

typedef struct spry_endpoint_args_optionalProperties_entry {
  sp_str_t key;
  spry_endpoint_arg_t value;
} spry_endpoint_args_optionalProperties_entry_t;

typedef struct spry_endpoint_args {
  sp_da(spry_endpoint_args_properties_entry_t) properties;
  sp_da(spry_endpoint_args_optionalProperties_entry_t) optionalProperties;
} spry_endpoint_args_t;

typedef struct spry_endpoint {
  spry_endpoint_args_t args;
} spry_endpoint_t;

typedef struct spry_endpoints_entry {
  sp_str_t key;
  spry_endpoint_t value;
} spry_endpoints_entry_t;

typedef sp_da(spry_endpoints_entry_t) spry_endpoints_t;

static const spry_ast_type_t spry_type_type;
static const spry_ast_type_t spry_endpoint_arg_type;
static const spry_ast_type_t spry_endpoint_args_properties_type;
static const spry_ast_type_t spry_endpoint_args_optionalProperties_type;
static const spry_ast_type_t spry_endpoint_args_type;
static const spry_ast_type_t spry_endpoint_type;
static const spry_ast_type_t spry_endpoints_type;

static const c8* const spry_type_names[] = { "string", "boolean", "int8", "uint8", "int16", "uint16", "int32", "uint32", "float32", "float64" };
static const spry_ast_type_t spry_type_type = { .kind = SPRY_AST_ENUM, .as.enom = { .names = spry_type_names, .count = 10 } };

static const spry_ast_field_t spry_endpoint_arg_fields[] = {
  { .key = "type", .offset = offsetof(spry_endpoint_arg_t, type), .type = &spry_type_type, .required = true, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t spry_endpoint_arg_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_endpoint_arg_fields, .count = 1 } };

static const spry_ast_type_t spry_endpoint_args_properties_type = { .kind = SPRY_AST_VALUES, .as.values = { .value = &spry_endpoint_arg_type, .stride = sizeof(spry_endpoint_args_properties_entry_t), .key_offset = offsetof(spry_endpoint_args_properties_entry_t, key), .value_offset = offsetof(spry_endpoint_args_properties_entry_t, value) } };

static const spry_ast_type_t spry_endpoint_args_optionalProperties_type = { .kind = SPRY_AST_VALUES, .as.values = { .value = &spry_endpoint_arg_type, .stride = sizeof(spry_endpoint_args_optionalProperties_entry_t), .key_offset = offsetof(spry_endpoint_args_optionalProperties_entry_t, key), .value_offset = offsetof(spry_endpoint_args_optionalProperties_entry_t, value) } };

static const spry_ast_field_t spry_endpoint_args_fields[] = {
  { .key = "properties", .offset = offsetof(spry_endpoint_args_t, properties), .type = &spry_endpoint_args_properties_type, .required = false, .is_ptr = false, .size = 0 },
  { .key = "optionalProperties", .offset = offsetof(spry_endpoint_args_t, optionalProperties), .type = &spry_endpoint_args_optionalProperties_type, .required = false, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t spry_endpoint_args_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_endpoint_args_fields, .count = 2 } };

static const spry_ast_field_t spry_endpoint_fields[] = {
  { .key = "args", .offset = offsetof(spry_endpoint_t, args), .type = &spry_endpoint_args_type, .required = false, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t spry_endpoint_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_endpoint_fields, .count = 1 } };

static const spry_ast_type_t spry_endpoints_type = { .kind = SPRY_AST_VALUES, .as.values = { .value = &spry_endpoint_type, .stride = sizeof(spry_endpoints_entry_t), .key_offset = offsetof(spry_endpoints_entry_t, key), .value_offset = offsetof(spry_endpoints_entry_t, value) } };

#endif
