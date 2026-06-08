#include "sp.h"
#include "abi.h"
#include "backend.h"
#include "stub_backend.h"

typedef struct { u32 next_id; } stub_t;

static u32 stub_caps(void* self) {
  (void)self;
  return ABI_REQUIRED_CAPS;
}

static void* stub_create(void* self, u32 kind) {
  stub_t* s = self;
  u32 id = ++s->next_id;
  sp_log("create kind={} -> #{}", sp_fmt_int(kind), sp_fmt_int(id));
  return (void*)(u64)id;
}

static void stub_set_attr(void* self, void* node, u32 attr, s32 value) {
  (void)self;
  sp_log("set_attr #{} attr={} val={}", sp_fmt_int((s64)(u64)node), sp_fmt_int(attr), sp_fmt_int(value));
}

static void stub_set_attr_str(void* self, void* node, u32 attr, sp_str_t value) {
  (void)self;
  sp_log("set_attr_str #{} attr={} \"{}\"", sp_fmt_int((s64)(u64)node), sp_fmt_int(attr), sp_fmt_str(value));
}

static void stub_append(void* self, void* parent, void* child) {
  (void)self;
  sp_log("append #{} -> #{}", sp_fmt_int((s64)(u64)child), sp_fmt_int((s64)(u64)parent));
}

static void stub_set_root(void* self, void* node) {
  (void)self;
  sp_log("set_root #{}", sp_fmt_int((s64)(u64)node));
}

static void stub_on_event(void* self, void* node, u32 event, u32 token) {
  (void)self;
  sp_log("on_event #{} event={} token={}", sp_fmt_int((s64)(u64)node), sp_fmt_int(event), sp_fmt_int(token));
}

static void stub_submit(void* self, u32 token, sp_str_t url, sp_str_t body) {
  (void)self;
  sp_log("submit token={} url=\"{}\" body=\"{}\"", sp_fmt_int(token), sp_fmt_str(url), sp_fmt_str(body));
}

static void stub_clear_children(void* self, void* node) {
  (void)self;
  sp_log("clear_children #{}", sp_fmt_int((s64)(u64)node));
}

static u32 stub_get_value(void* self, void* node, c8* out, u32 cap) {
  (void)self;
  (void)node;
  (void)out;
  (void)cap;
  return 0;
}

static void stub_fatal(void* self, sp_str_t message) {
  (void)self;
  sp_log("FATAL: {}", sp_fmt_str(message));
}

backend_t stub_backend_make(sp_mem_t mem) {
  stub_t* s = sp_alloc(mem, sizeof(stub_t));
  *s = sp_zero_s(stub_t);
  return (backend_t){
    .self = s,
    .capabilities = stub_caps,
    .create_element = stub_create,
    .set_attr = stub_set_attr,
    .set_attr_str = stub_set_attr_str,
    .append_child = stub_append,
    .set_root = stub_set_root,
    .on_event = stub_on_event,
    .submit = stub_submit,
    .clear_children = stub_clear_children,
    .get_value = stub_get_value,
    .fatal = stub_fatal,
  };
}
