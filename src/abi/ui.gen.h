#ifndef SPRY_UI_GEN_H
#define SPRY_UI_GEN_H

#include "spry/ast.h"
#include "abi/ui.enums.gen.h"

typedef struct spry_node spry_node_t;
typedef struct spry_box spry_box_t;
typedef struct spry_box_props spry_box_props_t;
typedef struct spry_interaction spry_interaction_t;
typedef struct spry_text spry_text_t;
typedef struct spry_text_props spry_text_props_t;
typedef struct spry_link spry_link_t;
typedef struct spry_link_props spry_link_props_t;
typedef struct spry_input spry_input_t;
typedef struct spry_input_props spry_input_props_t;
typedef struct spry_button spry_button_t;
typedef struct spry_button_props spry_button_props_t;

typedef struct spry_box_props {
  spry_direction_t direction;
  s32 gap;
  s32 padding;
  spry_align_t align;
  spry_justify_t justify;
} spry_box_props_t;

typedef struct spry_interaction {
  spry_event_t event;
  sp_str_t action;
  sp_str_t target;
  spry_swap_t swap;
} spry_interaction_t;

typedef struct spry_box {
  sp_str_t id;
  spry_box_props_t* props;
  spry_interaction_t* on;
  sp_da(spry_node_t) children;
} spry_box_t;

typedef struct spry_text_props {
  sp_str_t text;
} spry_text_props_t;

typedef struct spry_text {
  spry_text_props_t props;
  sp_str_t id;
} spry_text_t;

typedef struct spry_link_props {
  sp_str_t text;
  sp_str_t href;
} spry_link_props_t;

typedef struct spry_link {
  spry_link_props_t props;
  sp_str_t id;
} spry_link_t;

typedef struct spry_input_props {
  sp_str_t name;
  sp_str_t value;
  sp_str_t placeholder;
} spry_input_props_t;

typedef struct spry_input {
  spry_input_props_t props;
  sp_str_t id;
} spry_input_t;

typedef struct spry_button_props {
  sp_str_t text;
} spry_button_props_t;

typedef struct spry_button {
  spry_button_props_t props;
  sp_str_t id;
  spry_interaction_t* on;
} spry_button_t;

typedef struct spry_node {
  spry_node_kind_t kind;
  union {
    spry_box_t box;
    spry_text_t text;
    spry_link_t link;
    spry_input_t input;
    spry_button_t button;
  } as;
} spry_node_t;

typedef spry_node_t spry_ast_root_t;
#define SPRY_AST_ROOT_TYPE spry_node_type

static const spry_ast_type_t spry_bool_type = { .kind = SPRY_AST_BOOL };
static const spry_ast_type_t spry_i32_type = { .kind = SPRY_AST_I32 };
static const spry_ast_type_t spry_str_type = { .kind = SPRY_AST_STR };

static const spry_ast_type_t spry_direction_type;
static const spry_ast_type_t spry_align_type;
static const spry_ast_type_t spry_justify_type;
static const spry_ast_type_t spry_box_props_type;
static const spry_ast_type_t spry_event_type;
static const spry_ast_type_t spry_swap_type;
static const spry_ast_type_t spry_interaction_type;
static const spry_ast_type_t spry_node_array_type;
static const spry_ast_type_t spry_box_type;
static const spry_ast_type_t spry_text_props_type;
static const spry_ast_type_t spry_text_type;
static const spry_ast_type_t spry_link_props_type;
static const spry_ast_type_t spry_link_type;
static const spry_ast_type_t spry_input_props_type;
static const spry_ast_type_t spry_input_type;
static const spry_ast_type_t spry_button_props_type;
static const spry_ast_type_t spry_button_type;
static const spry_ast_type_t spry_node_type;

static const c8* const spry_direction_names[] = { "row", "column" };
static const spry_ast_type_t spry_direction_type = { .kind = SPRY_AST_ENUM, .as.enom = { .names = spry_direction_names, .count = 2 } };

static const c8* const spry_align_names[] = { "start", "center", "end", "stretch" };
static const spry_ast_type_t spry_align_type = { .kind = SPRY_AST_ENUM, .as.enom = { .names = spry_align_names, .count = 4 } };

static const c8* const spry_justify_names[] = { "start", "center", "end", "between" };
static const spry_ast_type_t spry_justify_type = { .kind = SPRY_AST_ENUM, .as.enom = { .names = spry_justify_names, .count = 4 } };

static const spry_ast_field_t spry_box_props_fields[] = {
  { .key = "direction", .offset = offsetof(spry_box_props_t, direction), .type = &spry_direction_type, .required = false, .is_ptr = false, .size = 0 },
  { .key = "gap", .offset = offsetof(spry_box_props_t, gap), .type = &spry_i32_type, .required = false, .is_ptr = false, .size = 0 },
  { .key = "padding", .offset = offsetof(spry_box_props_t, padding), .type = &spry_i32_type, .required = false, .is_ptr = false, .size = 0 },
  { .key = "align", .offset = offsetof(spry_box_props_t, align), .type = &spry_align_type, .required = false, .is_ptr = false, .size = 0 },
  { .key = "justify", .offset = offsetof(spry_box_props_t, justify), .type = &spry_justify_type, .required = false, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t spry_box_props_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_box_props_fields, .count = 5 } };

static const c8* const spry_event_names[] = { "click", "submit" };
static const spry_ast_type_t spry_event_type = { .kind = SPRY_AST_ENUM, .as.enom = { .names = spry_event_names, .count = 2 } };

static const c8* const spry_swap_names[] = { "inner" };
static const spry_ast_type_t spry_swap_type = { .kind = SPRY_AST_ENUM, .as.enom = { .names = spry_swap_names, .count = 1 } };

static const spry_ast_field_t spry_interaction_fields[] = {
  { .key = "event", .offset = offsetof(spry_interaction_t, event), .type = &spry_event_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "action", .offset = offsetof(spry_interaction_t, action), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "target", .offset = offsetof(spry_interaction_t, target), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "swap", .offset = offsetof(spry_interaction_t, swap), .type = &spry_swap_type, .required = false, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t spry_interaction_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_interaction_fields, .count = 4 } };

static const spry_ast_type_t spry_node_array_type = { .kind = SPRY_AST_ARRAY, .as.array = { .element = &spry_node_type, .stride = sizeof(spry_node_t) } };

static const spry_ast_field_t spry_box_fields[] = {
  { .key = "id", .offset = offsetof(spry_box_t, id), .type = &spry_str_type, .required = false, .is_ptr = false, .size = 0 },
  { .key = "props", .offset = offsetof(spry_box_t, props), .type = &spry_box_props_type, .required = false, .is_ptr = true, .size = sizeof(spry_box_props_t) },
  { .key = "on", .offset = offsetof(spry_box_t, on), .type = &spry_interaction_type, .required = false, .is_ptr = true, .size = sizeof(spry_interaction_t) },
  { .key = "children", .offset = offsetof(spry_box_t, children), .type = &spry_node_array_type, .required = false, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t spry_box_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_box_fields, .count = 4 } };

static const spry_ast_field_t spry_text_props_fields[] = {
  { .key = "text", .offset = offsetof(spry_text_props_t, text), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t spry_text_props_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_text_props_fields, .count = 1 } };

static const spry_ast_field_t spry_text_fields[] = {
  { .key = "props", .offset = offsetof(spry_text_t, props), .type = &spry_text_props_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "id", .offset = offsetof(spry_text_t, id), .type = &spry_str_type, .required = false, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t spry_text_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_text_fields, .count = 2 } };

static const spry_ast_field_t spry_link_props_fields[] = {
  { .key = "text", .offset = offsetof(spry_link_props_t, text), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "href", .offset = offsetof(spry_link_props_t, href), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t spry_link_props_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_link_props_fields, .count = 2 } };

static const spry_ast_field_t spry_link_fields[] = {
  { .key = "props", .offset = offsetof(spry_link_t, props), .type = &spry_link_props_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "id", .offset = offsetof(spry_link_t, id), .type = &spry_str_type, .required = false, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t spry_link_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_link_fields, .count = 2 } };

static const spry_ast_field_t spry_input_props_fields[] = {
  { .key = "name", .offset = offsetof(spry_input_props_t, name), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "value", .offset = offsetof(spry_input_props_t, value), .type = &spry_str_type, .required = false, .is_ptr = false, .size = 0 },
  { .key = "placeholder", .offset = offsetof(spry_input_props_t, placeholder), .type = &spry_str_type, .required = false, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t spry_input_props_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_input_props_fields, .count = 3 } };

static const spry_ast_field_t spry_input_fields[] = {
  { .key = "props", .offset = offsetof(spry_input_t, props), .type = &spry_input_props_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "id", .offset = offsetof(spry_input_t, id), .type = &spry_str_type, .required = false, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t spry_input_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_input_fields, .count = 2 } };

static const spry_ast_field_t spry_button_props_fields[] = {
  { .key = "text", .offset = offsetof(spry_button_props_t, text), .type = &spry_str_type, .required = true, .is_ptr = false, .size = 0 },
};
static const spry_ast_type_t spry_button_props_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_button_props_fields, .count = 1 } };

static const spry_ast_field_t spry_button_fields[] = {
  { .key = "props", .offset = offsetof(spry_button_t, props), .type = &spry_button_props_type, .required = true, .is_ptr = false, .size = 0 },
  { .key = "id", .offset = offsetof(spry_button_t, id), .type = &spry_str_type, .required = false, .is_ptr = false, .size = 0 },
  { .key = "on", .offset = offsetof(spry_button_t, on), .type = &spry_interaction_type, .required = false, .is_ptr = true, .size = sizeof(spry_interaction_t) },
};
static const spry_ast_type_t spry_button_type = { .kind = SPRY_AST_OBJECT, .as.object = { .fields = spry_button_fields, .count = 3 } };

static const spry_ast_variant_t spry_node_variants[] = {
  { .tag = "box", .value = SPRY_NODE_KIND_BOX, .type = &spry_box_type },
  { .tag = "text", .value = SPRY_NODE_KIND_TEXT, .type = &spry_text_type },
  { .tag = "link", .value = SPRY_NODE_KIND_LINK, .type = &spry_link_type },
  { .tag = "input", .value = SPRY_NODE_KIND_INPUT, .type = &spry_input_type },
  { .tag = "button", .value = SPRY_NODE_KIND_BUTTON, .type = &spry_button_type },
};
static const spry_ast_type_t spry_node_type = { .kind = SPRY_AST_UNION, .as.uni = { .tag_key = "kind", .tag_offset = offsetof(spry_node_t, kind), .payload_offset = offsetof(spry_node_t, as), .variants = spry_node_variants, .count = 5 } };

#endif
