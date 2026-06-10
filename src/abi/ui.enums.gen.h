#ifndef SPRY_UI_ENUMS_GEN_H
#define SPRY_UI_ENUMS_GEN_H

typedef enum {
  SPRY_NODE_KIND_BOX,
  SPRY_NODE_KIND_TEXT,
  SPRY_NODE_KIND_LINK,
  SPRY_NODE_KIND_INPUT,
  SPRY_NODE_KIND_BUTTON,
} spry_node_kind_t;

typedef enum {
  SPRY_DIRECTION_ROW,
  SPRY_DIRECTION_COLUMN,
} spry_direction_t;

typedef enum {
  SPRY_ALIGN_START,
  SPRY_ALIGN_CENTER,
  SPRY_ALIGN_END,
  SPRY_ALIGN_STRETCH,
} spry_align_t;

typedef enum {
  SPRY_JUSTIFY_START,
  SPRY_JUSTIFY_CENTER,
  SPRY_JUSTIFY_END,
  SPRY_JUSTIFY_BETWEEN,
} spry_justify_t;

typedef enum {
  SPRY_INTERACTION_KIND_INVOKE,
} spry_interaction_kind_t;

typedef enum {
  SPRY_EVENT_CLICK,
  SPRY_EVENT_SUBMIT,
} spry_event_t;

typedef enum {
  SPRY_ONRESPONSE_IGNORE,
  SPRY_ONRESPONSE_PATCH,
} spry_onResponse_t;

#endif
