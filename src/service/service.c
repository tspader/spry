#define SP_IMPLEMENTATION
#include "sp.h"

#include "yyjson.h"
#include "spry/core.h"

static sp_mem_t g_mem;
static bool g_mem_init = false;

static sp_mem_t svc_mem(void) {
  if (!g_mem_init) { g_mem = sp_mem_os_new(); g_mem_init = true; }
  return g_mem;
}

static sp_mem_arena_t* g_arena;
static sp_mem_t g_call_mem;
static bool g_arena_init = false;

static sp_mem_t svc_call_mem(void) {
  if (!g_arena_init) {
    g_arena = sp_mem_arena_new(svc_mem());
    g_call_mem = sp_mem_arena_as_allocator(g_arena);
    g_arena_init = true;
  }
  return g_call_mem;
}

static u8 g_json_pool[1u << 20];

static yyjson_doc* svc_parse(const c8* ptr, u32 len) {
  yyjson_alc alc;
  if (!yyjson_alc_pool_init(&alc, g_json_pool, sizeof(g_json_pool))) return SP_NULLPTR;
  return yyjson_read_opts((c8*)ptr, len, 0, &alc, SP_NULLPTR);
}

static spry_endpoints_t g_endpoints;
static sp_str_t g_result;

static u32 svc_fault_result(sp_mem_t mem, spry_fault_t* fault) {
  g_result = spry_fault_write(mem, fault);
  return spry_fault_status(fault->code);
}

__attribute__((export_name("begin")))
void svc_begin(void) {
  svc_call_mem();
  sp_mem_arena_clear(g_arena);
  g_result = sp_zero_s(sp_str_t);
}

__attribute__((export_name("alloc")))
void* svc_alloc(u32 len) {
  return sp_alloc(svc_call_mem(), len);
}

__attribute__((export_name("result_ptr")))
const c8* svc_result_ptr(void) {
  return g_result.data;
}

__attribute__((export_name("result_len")))
u32 svc_result_len(void) {
  return g_result.len;
}

__attribute__((export_name("endpoints")))
s32 svc_endpoints(const c8* ptr, u32 len) {
  g_endpoints = SP_NULLPTR;

  yyjson_doc* doc = svc_parse(ptr, len);
  if (!doc) {
    g_result = sp_str_lit("endpoints json parse error");
    return 1;
  }

  spry_issue_t issue = sp_zero_s(spry_issue_t);
  spry_endpoints_t endpoints = SP_NULLPTR;
  if (spry_endpoints_parse_val(svc_mem(), yyjson_doc_get_root(doc), &endpoints, &issue)) {
    g_result = spry_issue_str(svc_mem(), &issue);
    return 1;
  }

  g_endpoints = endpoints;
  return 0;
}

__attribute__((export_name("validate")))
u32 svc_validate(const c8* name_ptr, u32 name_len, const c8* body_ptr, u32 body_len) {
  sp_mem_t mem = svc_call_mem();
  sp_str_t name = sp_str(name_ptr, name_len);

  const spry_endpoint_t* ep = spry_endpoint_find(g_endpoints, name);
  if (!ep) {
    spry_fault_t fault = sp_zero_s(spry_fault_t);
    fault.code = SPRY_CODE_MISSING;
    fault.message = sp_fmt(mem, "unknown endpoint '{}'", sp_fmt_str(name)).value;
    return svc_fault_result(mem, &fault);
  }

  yyjson_doc* doc = svc_parse(body_ptr, body_len);
  if (!doc) {
    spry_fault_t fault = sp_zero_s(spry_fault_t);
    fault.code = SPRY_CODE_INVALID;
    fault.message = sp_str_lit("malformed JSON body");
    return svc_fault_result(mem, &fault);
  }

  spry_issues_t issues = spry_validate_args(mem, ep, yyjson_doc_get_root(doc));
  if (sp_da_size(issues)) {
    spry_fault_t fault = sp_zero_s(spry_fault_t);
    fault.code = SPRY_CODE_INVALID;
    fault.message = sp_str_lit("argument validation failed");
    fault.issues = issues;
    return svc_fault_result(mem, &fault);
  }

  return 0;
}

__attribute__((export_name("fault")))
u32 svc_fault(const c8* code_ptr, u32 code_len, const c8* msg_ptr, u32 msg_len, const c8* issues_ptr, u32 issues_len) {
  sp_mem_t mem = svc_call_mem();

  spry_code_t code;
  if (!spry_code_parse(sp_str(code_ptr, code_len), &code) || !spry_fault_status(code)) {
    code = SPRY_CODE_FAILED;
  }

  spry_fault_t fault = sp_zero_s(spry_fault_t);
  fault.code = code;
  fault.message = sp_str(msg_ptr, msg_len);

  if (issues_len) {
    yyjson_doc* doc = svc_parse(issues_ptr, issues_len);
    if (doc) {
      spry_ctx_t ctx;
      spry_ctx_init(&ctx, mem);
      sp_da(spry_fault_issue_t) issues = SP_NULLPTR;
      if (spry_ast_parse(&spry_fault_issue_array_type, yyjson_doc_get_root(doc), &ctx, &issues) == SPRY_OK) {
        fault.issues = issues;
      }
    }
  }

  return svc_fault_result(mem, &fault);
}

__attribute__((export_name("fragment")))
s32 svc_fragment(const c8* ptr, u32 len) {
  sp_mem_t mem = svc_call_mem();

  yyjson_doc* doc = svc_parse(ptr, len);
  if (!doc) {
    g_result = sp_str_lit("fragment json parse error");
    return 1;
  }

  spry_node_t node = sp_zero_s(spry_node_t);
  sp_str_t error = sp_zero_s(sp_str_t);
  if (!spry_node_parse_val(mem, yyjson_doc_get_root(doc), &node, &error)) {
    g_result = error;
    return 1;
  }

  return 0;
}

void* malloc(size_t size) {
  return sp_sys_alloc(size);
}

void* realloc(void* ptr, size_t size) {
  (void)ptr; (void)size;
  __builtin_trap();
}

void free(void* ptr) {
  (void)ptr;
}

size_t strlen(const char* s) {
  const char* p = s;
  while (*p) p++;
  return (size_t)(p - s);
}

__int128 __multi3(__int128 a, __int128 b) {
  return a * b;
}
