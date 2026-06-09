#include "sp.h"
#include "spry/abi.h"

extern s32  rt_render(const c8* ptr, u32 len);
extern void rt_dispatch(u32 token);
extern void rt_deliver(u32 token, const c8* ptr, u32 len);

static const c8* EL_NAMES[]    = { "box", "text", "link", "input", "button" };
static const c8* EVENT_NAMES[] = { "click", "submit" };

static u32 g_handle;

u32 host_capabilities(void) {
  return ABI_REQUIRED_CAPS;
}

u32 host_create_element(u32 kind) {
  g_handle += 1;
  sp_log("create   #{} {}", sp_fmt_uint(g_handle), sp_fmt_cstr(EL_NAMES[kind]));
  return g_handle;
}

static void log_set_int(u32 handle, const c8* name, s32 value) {
  sp_log("{} #{} = {}", sp_fmt_cstr(name), sp_fmt_uint(handle), sp_fmt_int(value));
}

static void log_set_str(u32 handle, const c8* name, const c8* ptr, u32 len) {
  sp_str_t value = { .data = ptr, .len = len };
  sp_log("{} #{} = '{}'", sp_fmt_cstr(name), sp_fmt_uint(handle), sp_fmt_str(value));
}

void host_set_direction(u32 handle, u32 value) { log_set_int(handle, "set_direction", (s32)value); }
void host_set_gap(u32 handle, s32 value) { log_set_int(handle, "set_gap", value); }
void host_set_padding(u32 handle, s32 value) { log_set_int(handle, "set_padding", value); }
void host_set_align(u32 handle, u32 value) { log_set_int(handle, "set_align", (s32)value); }
void host_set_justify(u32 handle, u32 value) { log_set_int(handle, "set_justify", (s32)value); }
void host_set_text(u32 handle, const c8* ptr, u32 len) { log_set_str(handle, "set_text", ptr, len); }
void host_set_href(u32 handle, const c8* ptr, u32 len) { log_set_str(handle, "set_href", ptr, len); }
void host_set_value(u32 handle, const c8* ptr, u32 len) { log_set_str(handle, "set_value", ptr, len); }
void host_set_name(u32 handle, const c8* ptr, u32 len) { log_set_str(handle, "set_name", ptr, len); }
void host_set_placeholder(u32 handle, const c8* ptr, u32 len) { log_set_str(handle, "set_placeholder", ptr, len); }

void host_append_child(u32 parent, u32 child) {
  sp_log("append   #{} <- #{}", sp_fmt_uint(parent), sp_fmt_uint(child));
}

void host_set_root(u32 handle) {
  sp_log("set_root #{}", sp_fmt_uint(handle));
}

void host_on_event(u32 handle, u32 event, u32 token) {
  sp_log("on_event #{} {} token={}", sp_fmt_uint(handle), sp_fmt_cstr(EVENT_NAMES[event]), sp_fmt_uint(token));
}

void host_submit(u32 token, const c8* action_ptr, u32 action_len, const c8* body_ptr, u32 body_len) {
  sp_str_t action = { .data = action_ptr, .len = action_len };
  sp_str_t body   = { .data = body_ptr, .len = body_len };
  sp_log("submit   token={} action='{}' body='{}'", sp_fmt_uint(token), sp_fmt_str(action), sp_fmt_str(body));
}

void host_clear_children(u32 handle) {
  sp_log("clear    #{}", sp_fmt_uint(handle));
}

u32 host_get_value(u32 handle, c8* out_ptr, u32 cap) {
  sp_str_t canned = sp_str_lit("alice");
  u32 n = canned.len < cap ? canned.len : cap;
  sp_for(i, n) out_ptr[i] = canned.data[i];
  sp_log("get_value #{} -> '{}'", sp_fmt_uint(handle), sp_fmt_str(canned));
  return n;
}

void host_fatal(const c8* ptr, u32 len) {
  sp_str_t msg = { .data = ptr, .len = len };
  sp_log("FATAL    {}", sp_fmt_str(msg));
}

s32 main(s32 argc, const c8** argv) {
  if (argc < 2) {
    sp_log("usage: runtime_harness <tree.json>");
    return 1;
  }

  sp_mem_t mem = sp_mem_os_new();
  sp_str_t tree = sp_zero_s(sp_str_t);
  if (sp_io_read_file(mem, sp_cstr_as_str(argv[1]), &tree) != SP_OK) {
    sp_log("cannot read {}", sp_fmt_cstr(argv[1]));
    return 1;
  }

  sp_log("== render(tree) ==");
  s32 rc = rt_render(tree.data, tree.len);
  sp_log("render rc = {}", sp_fmt_int(rc));

  sp_log("== dispatch(token 0) ==");
  rt_dispatch(0);

  sp_log("== deliver(token 0, greet fragment) ==");
  sp_str_t frag = sp_str_lit("{\"kind\":\"text\",\"props\":{\"text\":\"Hello, alice!\"}}");
  rt_deliver(0, frag.data, frag.len);

  sp_log("== reject(bad blob) ==");
  sp_str_t bad = sp_str_lit("{\"kind\":\"box\",\"props\":{\"align\":\"diagonal\"}}");
  s32 rc_bad = rt_render(bad.data, bad.len);
  sp_log("render rc = {}", sp_fmt_int(rc_bad));

  return 0;
}
