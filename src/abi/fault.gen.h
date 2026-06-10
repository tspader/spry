#ifndef SPRY_FAULT_GEN_H
#define SPRY_FAULT_GEN_H

#include "spry/ast.h"
#include "abi/fault.enums.gen.h"

typedef struct spry_fault spry_fault_t;
typedef struct spry_fault_issue spry_fault_issue_t;

typedef struct spry_fault_issue {
  sp_str_t path;
  sp_str_t code;
} spry_fault_issue_t;

typedef struct spry_fault {
  spry_code_t code;
  sp_str_t message;
  sp_da(spry_fault_issue_t) issues;
} spry_fault_t;

#ifndef SPRY_GEN_SCALAR_BOOL
#define SPRY_GEN_SCALAR_BOOL
static const spry_ast_type_t spry_bool_type = { .kind = SPRY_AST_BOOL };
#endif

#ifndef SPRY_GEN_SCALAR_STR
#define SPRY_GEN_SCALAR_STR
static const spry_ast_type_t spry_str_type = { .kind = SPRY_AST_STR };
#endif

static const spry_ast_type_t spry_code_type;
static const spry_ast_type_t spry_fault_issue_type;
static const spry_ast_type_t spry_fault_issue_array_type;
static const spry_ast_type_t spry_fault_type;

static const c8* const spry_code_names[] = { "invalid", "unauthenticated", "denied", "missing", "conflict", "failed", "unavailable", "timeout", "cancelled" };
static const spry_ast_type_t spry_code_type = { .kind = SPRY_AST_ENUM, .as.enom = { .names = spry_code_names, .count = 9 } };

static const spry_ast_field_t spry_fault_issue_fields[] = {
  { .key = "path", .offset = offsetof(spry_fault_issue_t, path), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "code", .offset = offsetof(spry_fault_issue_t, code), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t spry_fault_issue_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_fault_issue_fields, .count = 2 } };

static const spry_ast_type_t spry_fault_issue_array_type = { .kind = SPRY_AST_ARRAY, .as.array = { .element = &spry_fault_issue_type, .stride = sizeof(spry_fault_issue_t) } };

static const spry_ast_field_t spry_fault_fields[] = {
  { .key = "code", .offset = offsetof(spry_fault_t, code), .type = &spry_code_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "message", .offset = offsetof(spry_fault_t, message), .type = &spry_str_type, .required = false, .is_ptr = false, .size = 0 },
  { .key = "issues", .offset = offsetof(spry_fault_t, issues), .type = &spry_fault_issue_array_type, .required = false, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t spry_fault_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_fault_fields, .count = 3 } };

#endif
