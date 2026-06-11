#ifndef SPRY_CORE_H
#define SPRY_CORE_H

#include "sp.h"
#include "yyjson.h"
#include "spry/ast.h"
#include "abi/ui.gen.h"
#include "abi/fault.gen.h"
#include "abi/endpoints.gen.h"

typedef sp_da(spry_fault_issue_t) spry_issues_t;

static inline sp_str_t spry_str_pass(sp_str_t s) {
  return s;
}

#define spry_str(s) _Generic((s), char*: sp_cstr_as_str, const char*: sp_cstr_as_str, sp_str_t: spry_str_pass)(s)

spry_err_t                 spry_endpoints_parse(sp_mem_t mem, sp_str_t json, spry_endpoints_t* out, spry_issue_t* issue);
spry_err_t                 spry_endpoints_parse_val(sp_mem_t mem, yyjson_val* val, spry_endpoints_t* out, spry_issue_t* issue);
const spry_endpoint_t*     spry_endpoint_find(spry_endpoints_t endpoints, sp_str_t name);
const spry_endpoint_arg_t* spry_endpoint_arg_find(const spry_endpoint_t* ep, sp_str_t name);

bool          spry_coercible(spry_type_t type, sp_str_t value);
spry_issues_t spry_validate_args(sp_mem_t mem, const spry_endpoint_t* ep, yyjson_val* body);

sp_str_t    spry_fault_write(sp_mem_t mem, const spry_fault_t* fault);
bool        spry_fault_parse_val(sp_mem_t mem, yyjson_val* val, spry_fault_t* out);
bool        spry_code_parse(sp_str_t name, spry_code_t* out);
u16         spry_fault_status(spry_code_t code);
spry_code_t spry_intermediary_code(u32 status);

bool                      spry_node_parse(sp_mem_t mem, sp_str_t json, spry_node_t* out, sp_str_t* error);
bool                      spry_node_parse_val(sp_mem_t mem, yyjson_val* val, spry_node_t* out, sp_str_t* error);
sp_str_t                  spry_node_id(const spry_node_t* node);
const spry_interaction_t* spry_node_on(const spry_node_t* node);

#endif
