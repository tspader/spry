#include "sp.h"
#include "spry/abi.h"
#include "spry/backend/backend.h"
#include "spry/backend/stub.h"

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

static void stub_set_int(void* node, const c8* name, s32 value) {
  sp_log("{} #{} = {}", sp_fmt_cstr(name), sp_fmt_int((s64)(u64)node), sp_fmt_int(value));
}

static void stub_set_str(void* node, const c8* name, sp_str_t value) {
  sp_log("{} #{} = \"{}\"", sp_fmt_cstr(name), sp_fmt_int((s64)(u64)node), sp_fmt_str(value));
}

static void stub_set_direction(void* self, void* node, u32 value) { (void)self; stub_set_int(node, "set_direction", (s32)value); }
static void stub_set_gap(void* self, void* node, s32 value) { (void)self; stub_set_int(node, "set_gap", value); }
static void stub_set_padding(void* self, void* node, s32 value) { (void)self; stub_set_int(node, "set_padding", value); }
static void stub_set_align(void* self, void* node, u32 value) { (void)self; stub_set_int(node, "set_align", (s32)value); }
static void stub_set_justify(void* self, void* node, u32 value) { (void)self; stub_set_int(node, "set_justify", (s32)value); }
static void stub_set_text(void* self, void* node, sp_str_t value) { (void)self; stub_set_str(node, "set_text", value); }
static void stub_set_href(void* self, void* node, sp_str_t value) { (void)self; stub_set_str(node, "set_href", value); }
static void stub_set_value(void* self, void* node, sp_str_t value) { (void)self; stub_set_str(node, "set_value", value); }
static void stub_set_name(void* self, void* node, sp_str_t value) { (void)self; stub_set_str(node, "set_name", value); }
static void stub_set_placeholder(void* self, void* node, sp_str_t value) { (void)self; stub_set_str(node, "set_placeholder", value); }

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

static void stub_invoke(void* self, u32 token, sp_str_t handler, sp_str_t body) {
  (void)self;
  sp_log("invoke token={} handler=\"{}\" body=\"{}\"", sp_fmt_int(token), sp_fmt_str(handler), sp_fmt_str(body));
}

static void stub_report(void* self, u32 token, sp_str_t fault) {
  (void)self;
  sp_log("report token={} fault={}", sp_fmt_int(token), sp_fmt_str(fault));
}

static void stub_clear_children(void* self, void* node) {
  (void)self;
  sp_log("clear_children #{}", sp_fmt_int((s64)(u64)node));
}

static u32 stub_get_value(void* self, void* node, c8* out, u32 cap) {
  (void)self;
  (void)node;
  sp_str_t canned = sp_str_lit("alice");
  u32 n = sp_min(canned.len, cap);
  sp_for(i, n) out[i] = canned.data[i];
  return n;
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
    .set_direction = stub_set_direction,
    .set_gap = stub_set_gap,
    .set_padding = stub_set_padding,
    .set_align = stub_set_align,
    .set_justify = stub_set_justify,
    .set_text = stub_set_text,
    .set_href = stub_set_href,
    .set_value = stub_set_value,
    .set_name = stub_set_name,
    .set_placeholder = stub_set_placeholder,
    .append_child = stub_append,
    .set_root = stub_set_root,
    .on_event = stub_on_event,
    .invoke = stub_invoke,
    .report = stub_report,
    .clear_children = stub_clear_children,
    .get_value = stub_get_value,
    .fatal = stub_fatal,
  };
}
