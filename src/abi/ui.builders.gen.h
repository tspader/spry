#ifndef SPRY_UI_BUILDERS_GEN_H
#define SPRY_UI_BUILDERS_GEN_H

#include "abi/ui.gen.h"

typedef struct spry_ui spry_ui_t;

u32 spry_ui_enter(spry_ui_t* ui, spry_node_t node);
void spry_ui_set_on(spry_ui_t* ui, u32 node, spry_interaction_t on);

typedef struct spry_box_decl {
  const void* spry_ui__designated;
  spry_direction_t direction;
  s32 gap;
  s32 padding;
  spry_align_t align;
  spry_justify_t justify;
  sp_str_t id;
  const spry_interaction_t* on;
} spry_box_decl_t;

typedef struct spry_text_decl {
  const void* spry_ui__designated;
  sp_str_t text;
  sp_str_t id;
} spry_text_decl_t;

typedef struct spry_link_decl {
  const void* spry_ui__designated;
  sp_str_t text;
  sp_str_t href;
  sp_str_t id;
} spry_link_decl_t;

typedef struct spry_input_decl {
  const void* spry_ui__designated;
  sp_str_t name;
  sp_str_t value;
  sp_str_t placeholder;
  sp_str_t id;
} spry_input_decl_t;

typedef struct spry_button_decl {
  const void* spry_ui__designated;
  sp_str_t text;
  sp_str_t id;
  const spry_interaction_t* on;
} spry_button_decl_t;

static inline u32 spry_ui_open_box(spry_ui_t* ui, spry_box_decl_t decl) {
  spry_node_t node = sp_zero_s(spry_node_t);
  node.kind = SPRY_NODE_KIND_BOX;
  node.as.box.id = decl.id;
  node.as.box.props.direction = decl.direction;
  node.as.box.props.gap = decl.gap;
  node.as.box.props.padding = decl.padding;
  node.as.box.props.align = decl.align;
  node.as.box.props.justify = decl.justify;
  u32 opened = spry_ui_enter(ui, node);
  if (decl.on) spry_ui_set_on(ui, opened, *decl.on);
  return opened;
}

static inline u32 spry_ui_open_text(spry_ui_t* ui, spry_text_decl_t decl) {
  spry_node_t node = sp_zero_s(spry_node_t);
  node.kind = SPRY_NODE_KIND_TEXT;
  node.as.text.id = decl.id;
  node.as.text.props.text = decl.text;
  u32 opened = spry_ui_enter(ui, node);
  return opened;
}

static inline u32 spry_ui_open_link(spry_ui_t* ui, spry_link_decl_t decl) {
  spry_node_t node = sp_zero_s(spry_node_t);
  node.kind = SPRY_NODE_KIND_LINK;
  node.as.link.id = decl.id;
  node.as.link.props.text = decl.text;
  node.as.link.props.href = decl.href;
  u32 opened = spry_ui_enter(ui, node);
  return opened;
}

static inline u32 spry_ui_open_input(spry_ui_t* ui, spry_input_decl_t decl) {
  spry_node_t node = sp_zero_s(spry_node_t);
  node.kind = SPRY_NODE_KIND_INPUT;
  node.as.input.id = decl.id;
  node.as.input.props.name = decl.name;
  node.as.input.props.value = decl.value;
  node.as.input.props.placeholder = decl.placeholder;
  u32 opened = spry_ui_enter(ui, node);
  return opened;
}

static inline u32 spry_ui_open_button(spry_ui_t* ui, spry_button_decl_t decl) {
  spry_node_t node = sp_zero_s(spry_node_t);
  node.kind = SPRY_NODE_KIND_BUTTON;
  node.as.button.id = decl.id;
  node.as.button.props.text = decl.text;
  u32 opened = spry_ui_enter(ui, node);
  if (decl.on) spry_ui_set_on(ui, opened, *decl.on);
  return opened;
}

#define SPRY_BOX(...) SPRY_UI_ELEMENT(spry_ui_open_box(spry_ui__ctx, (spry_box_decl_t){ __VA_ARGS__ }))

#define SPRY_TEXT(spry__content, ...) SPRY_UI_ELEMENT(spry_ui_open_text(spry_ui__ctx, (spry_text_decl_t){ .text = spry_ui_str(spry__content), __VA_ARGS__ }))

#define SPRY_LINK(...) SPRY_UI_ELEMENT(spry_ui_open_link(spry_ui__ctx, (spry_link_decl_t){ __VA_ARGS__ }))

#define SPRY_INPUT(...) SPRY_UI_ELEMENT(spry_ui_open_input(spry_ui__ctx, (spry_input_decl_t){ __VA_ARGS__ }))

#define SPRY_BUTTON(spry__content, ...) SPRY_UI_ELEMENT(spry_ui_open_button(spry_ui__ctx, (spry_button_decl_t){ .text = spry_ui_str(spry__content), __VA_ARGS__ }))

#define SPRY_ROW(...) SPRY_UI_ELEMENT(spry_ui_open_box(spry_ui__ctx, (spry_box_decl_t){ .direction = SPRY_DIRECTION_ROW, __VA_ARGS__ }))

#define SPRY_COLUMN(...) SPRY_UI_ELEMENT(spry_ui_open_box(spry_ui__ctx, (spry_box_decl_t){ .direction = SPRY_DIRECTION_COLUMN, __VA_ARGS__ }))

#define SPRY_TEXTF(...) SPRY_UI_ELEMENT(spry_ui_open_text(spry_ui__ctx, (spry_text_decl_t){ .text = spry_ui_fmt(spry_ui__ctx, __VA_ARGS__) }))

#define SPRY_INVOKE(...) (&(const spry_interaction_t){ .kind = SPRY_INTERACTION_KIND_INVOKE, .as.invoke = { __VA_ARGS__ } })

#define SPRY_PATCH(spry__handler, spry__target) SPRY_INVOKE(.handler = spry_ui_str(spry__handler), .target = spry_ui_str(spry__target), .onResponse = SPRY_ONRESPONSE_PATCH)

#endif
