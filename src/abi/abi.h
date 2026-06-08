#ifndef ABI_H
#define ABI_H

#define ABI_VERSION 1

typedef enum {
  HOST_CAP_ELEMENT = 1 << 0,
  HOST_CAP_TEXT    = 1 << 1,
  HOST_CAP_FLEX    = 1 << 2,
  HOST_CAP_LINK    = 1 << 3,
  HOST_CAP_INPUT   = 1 << 4,
  HOST_CAP_EVENTS  = 1 << 5,
  HOST_CAP_SUBMIT  = 1 << 6,
} host_cap_t;

#define ABI_REQUIRED_CAPS (HOST_CAP_ELEMENT | HOST_CAP_TEXT | HOST_CAP_FLEX | HOST_CAP_LINK | HOST_CAP_INPUT | HOST_CAP_EVENTS | HOST_CAP_SUBMIT)

typedef enum {
  EL_BOX    = 0,
  EL_TEXT   = 1,
  EL_LINK   = 2,
  EL_INPUT  = 3,
  EL_BUTTON = 4,
} el_kind_t;

typedef enum {
  ATTR_DIRECTION = 0,
  ATTR_GAP       = 1,
  ATTR_PADDING   = 2,
  ATTR_ALIGN     = 3,
  ATTR_JUSTIFY   = 4,
} attr_t;

typedef enum { DIR_ROW = 0, DIR_COLUMN = 1 } direction_t;
typedef enum { ALIGN_START = 0, ALIGN_CENTER = 1, ALIGN_END = 2, ALIGN_STRETCH = 3 } align_t;
typedef enum { JUSTIFY_START = 0, JUSTIFY_CENTER = 1, JUSTIFY_END = 2, JUSTIFY_BETWEEN = 3 } justify_t;

typedef enum {
  SATTR_TEXT        = 0,
  SATTR_HREF        = 1,
  SATTR_VALUE       = 2,
  SATTR_NAME        = 3,
  SATTR_PLACEHOLDER = 4,
} sattr_t;

typedef enum {
  EVENT_CLICK  = 0,
  EVENT_SUBMIT = 1,
} event_kind_t;

#define HANDLE_NONE 0

#ifdef ABI_RUNTIME

#define ABI_IMPORT(name) __attribute__((import_module("host"), import_name(#name)))

ABI_IMPORT(capabilities)   u32  host_capabilities(void);
ABI_IMPORT(create)         u32  host_create_element(u32 kind);
ABI_IMPORT(set_attr)       void host_set_attr(u32 handle, u32 attr, s32 value);
ABI_IMPORT(set_attr_str)   void host_set_attr_str(u32 handle, u32 attr, const c8* ptr, u32 len);
ABI_IMPORT(append_child)   void host_append_child(u32 parent, u32 child);
ABI_IMPORT(set_root)       void host_set_root(u32 handle);
ABI_IMPORT(on_event)       void host_on_event(u32 handle, u32 event, u32 token);
ABI_IMPORT(submit)         void host_submit(u32 token, const c8* action_ptr, u32 action_len, const c8* body_ptr, u32 body_len);
ABI_IMPORT(clear_children) void host_clear_children(u32 handle);
ABI_IMPORT(get_value)      u32  host_get_value(u32 handle, c8* out_ptr, u32 cap);
ABI_IMPORT(fatal)          void host_fatal(const c8* ptr, u32 len);

#endif

#endif
