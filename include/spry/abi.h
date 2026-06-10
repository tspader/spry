#ifndef ABI_H
#define ABI_H

#define ABI_VERSION 3

typedef enum {
  HOST_CAP_ELEMENT = 1 << 0,
  HOST_CAP_TEXT    = 1 << 1,
  HOST_CAP_FLEX    = 1 << 2,
  HOST_CAP_LINK    = 1 << 3,
  HOST_CAP_INPUT   = 1 << 4,
  HOST_CAP_EVENTS  = 1 << 5,
  HOST_CAP_INVOKE  = 1 << 6,
  HOST_CAP_REPORT  = 1 << 7,
} host_cap_t;

typedef enum {
  DELIVER_OK          = 0,
  DELIVER_FAULT       = 1,
  DELIVER_UNREACHABLE = 2,
  DELIVER_TIMEOUT     = 3,
  DELIVER_CANCELLED   = 4,
} deliver_outcome_t;

#define ABI_REQUIRED_CAPS (HOST_CAP_ELEMENT | HOST_CAP_TEXT | HOST_CAP_FLEX | HOST_CAP_LINK | HOST_CAP_INPUT | HOST_CAP_EVENTS | HOST_CAP_INVOKE | HOST_CAP_REPORT)

#define HANDLE_NONE 0

#ifdef ABI_RUNTIME

#define ABI_IMPORT(name) __attribute__((import_module("host"), import_name(#name)))

ABI_IMPORT(capabilities)   u32  host_capabilities(void);
ABI_IMPORT(create)         u32  host_create_element(u32 kind);
ABI_IMPORT(set_direction)  void host_set_direction(u32 handle, u32 value);
ABI_IMPORT(set_gap)        void host_set_gap(u32 handle, s32 value);
ABI_IMPORT(set_padding)    void host_set_padding(u32 handle, s32 value);
ABI_IMPORT(set_align)      void host_set_align(u32 handle, u32 value);
ABI_IMPORT(set_justify)    void host_set_justify(u32 handle, u32 value);
ABI_IMPORT(set_text)       void host_set_text(u32 handle, const c8* ptr, u32 len);
ABI_IMPORT(set_href)       void host_set_href(u32 handle, const c8* ptr, u32 len);
ABI_IMPORT(set_value)      void host_set_value(u32 handle, const c8* ptr, u32 len);
ABI_IMPORT(set_name)       void host_set_name(u32 handle, const c8* ptr, u32 len);
ABI_IMPORT(set_placeholder) void host_set_placeholder(u32 handle, const c8* ptr, u32 len);
ABI_IMPORT(append_child)   void host_append_child(u32 parent, u32 child);
ABI_IMPORT(set_root)       void host_set_root(u32 handle);
ABI_IMPORT(on_event)       void host_on_event(u32 handle, u32 event, u32 token);
ABI_IMPORT(invoke)         void host_invoke(u32 token, const c8* handler_ptr, u32 handler_len, const c8* body_ptr, u32 body_len);
ABI_IMPORT(report)         void host_report(u32 token, const c8* ptr, u32 len);
ABI_IMPORT(clear_children) void host_clear_children(u32 handle);
ABI_IMPORT(get_value)      u32  host_get_value(u32 handle, c8* out_ptr, u32 cap);
ABI_IMPORT(fatal)          void host_fatal(const c8* ptr, u32 len);

#endif

#endif
