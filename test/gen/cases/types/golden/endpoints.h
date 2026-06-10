#ifndef TEST_ENDPOINTS_GEN_H
#define TEST_ENDPOINTS_GEN_H

#include "spry/core.h"
#include "spry/rpc.h"

typedef struct {
  sp_str_t a;
  bool b;
  s8 c;
  u8 d;
  s16 e;
  u16 f;
  s32 g;
  u32 h;
  f32 i;
  f64 j;
} test_scan_args_t;

static const spry_ast_field_t test_scan_args_fields[] = {
  { .key = "a", .offset = offsetof(test_scan_args_t, a), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "b", .offset = offsetof(test_scan_args_t, b), .type = &spry_bool_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "c", .offset = offsetof(test_scan_args_t, c), .type = &spry_s8_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "d", .offset = offsetof(test_scan_args_t, d), .type = &spry_u8_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "e", .offset = offsetof(test_scan_args_t, e), .type = &spry_s16_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "f", .offset = offsetof(test_scan_args_t, f), .type = &spry_u16_type, .required = false, .is_ptr = false, .size = 0 },
  { .key = "g", .offset = offsetof(test_scan_args_t, g), .type = &spry_s32_type, .required = false, .is_ptr = false, .size = 0 },
  { .key = "h", .offset = offsetof(test_scan_args_t, h), .type = &spry_u32_type, .required = false, .is_ptr = false, .size = 0 },
  { .key = "i", .offset = offsetof(test_scan_args_t, i), .type = &spry_f32_type, .required = false, .is_ptr = false, .size = 0 },
  { .key = "j", .offset = offsetof(test_scan_args_t, j), .type = &spry_f64_type, .required = false, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t test_scan_args_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = test_scan_args_fields, .count = 10 } };

typedef spry_reply_t (*test_scan_fn_t)(void* ctx, const test_scan_args_t* args);

static inline spry_reply_t test_scan_thunk(void* ctx, const spry_args_t* args) {
  spry_binding_t* binding = ctx;
  test_scan_args_t out = sp_zero_s(test_scan_args_t);
  spry_reply_t fault;
  if (!spry_rpc_parse(binding, args, &test_scan_args_type, &out, &fault)) return fault;
  return ((test_scan_fn_t)binding->fn)(binding->ctx, &out);
}

static inline void test_register_scan(spry_rpc_t* rpc, test_scan_fn_t fn, void* ctx) {
  spry_rpc_bind(rpc, sp_str_lit("scan"), (spry_handler_any_t)fn, ctx, test_scan_thunk);
}

#endif
