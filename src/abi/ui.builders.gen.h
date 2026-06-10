#ifndef SPRY_UI_BUILDERS_GEN_H
#define SPRY_UI_BUILDERS_GEN_H

#include "abi/ui.gen.h"

typedef struct spry_ui spry_ui_t;

u32 spry_ui_push(spry_ui_t* ui, spry_node_t node);
void spry_ui_set_on(spry_ui_t* ui, u32 node, spry_interaction_t on);

static inline u32 spry_ui_box(spry_ui_t* ui, spry_box_props_t props) {
  spry_node_t node = sp_zero_s(spry_node_t);
  node.kind = SPRY_NODE_KIND_BOX;
  node.as.box.props = props;
  return spry_ui_push(ui, node);
}

static inline u32 spry_ui_text(spry_ui_t* ui, spry_text_props_t props) {
  spry_node_t node = sp_zero_s(spry_node_t);
  node.kind = SPRY_NODE_KIND_TEXT;
  node.as.text.props = props;
  return spry_ui_push(ui, node);
}

static inline u32 spry_ui_link(spry_ui_t* ui, spry_link_props_t props) {
  spry_node_t node = sp_zero_s(spry_node_t);
  node.kind = SPRY_NODE_KIND_LINK;
  node.as.link.props = props;
  return spry_ui_push(ui, node);
}

static inline u32 spry_ui_input(spry_ui_t* ui, spry_input_props_t props) {
  spry_node_t node = sp_zero_s(spry_node_t);
  node.kind = SPRY_NODE_KIND_INPUT;
  node.as.input.props = props;
  return spry_ui_push(ui, node);
}

static inline u32 spry_ui_button(spry_ui_t* ui, spry_button_props_t props) {
  spry_node_t node = sp_zero_s(spry_node_t);
  node.kind = SPRY_NODE_KIND_BUTTON;
  node.as.button.props = props;
  return spry_ui_push(ui, node);
}

static inline void spry_ui_invoke(spry_ui_t* ui, u32 node, spry_invoke_t invoke) {
  spry_interaction_t on = sp_zero_s(spry_interaction_t);
  on.kind = SPRY_INTERACTION_KIND_INVOKE;
  on.as.invoke = invoke;
  spry_ui_set_on(ui, node, on);
}

#endif
