#include "spry/ui.h"
#include "spry/ast.h"

typedef struct {
  spry_node_t node;
  sp_da(u32) children;
} spry_ui_entry_t;

struct spry_ui {
  sp_mem_t mem;
  sp_da(spry_ui_entry_t) entries;
};

spry_ui_t* spry_ui_new(sp_mem_t mem) {
  spry_ui_t* ui = sp_alloc_type(mem, spry_ui_t);
  *ui = sp_zero_s(spry_ui_t);
  ui->mem = mem;
  ui->entries = sp_da_new(mem, spry_ui_entry_t);
  return ui;
}

static spry_node_t* spry_ui_node_at(spry_ui_t* ui, u32 node) {
  SP_ASSERT(node < sp_da_size(ui->entries));
  return &ui->entries[node].node;
}

u32 spry_ui_push(spry_ui_t* ui, spry_node_t node) {
  spry_ui_entry_t entry = sp_zero_s(spry_ui_entry_t);
  entry.node = node;
  entry.children = sp_da_new(ui->mem, u32);
  sp_da_push(ui->entries, entry);
  return (u32)(sp_da_size(ui->entries) - 1);
}

void spry_ui_id(spry_ui_t* ui, u32 node, sp_str_t id) {
  spry_node_t* n = spry_ui_node_at(ui, node);
  switch (n->kind) {
    case SPRY_NODE_KIND_BOX:    n->as.box.id = id;    return;
    case SPRY_NODE_KIND_TEXT:   n->as.text.id = id;   return;
    case SPRY_NODE_KIND_LINK:   n->as.link.id = id;   return;
    case SPRY_NODE_KIND_INPUT:  n->as.input.id = id;  return;
    case SPRY_NODE_KIND_BUTTON: n->as.button.id = id; return;
  }
}

void spry_ui_set_on(spry_ui_t* ui, u32 node, spry_interaction_t on) {
  spry_node_t* n = spry_ui_node_at(ui, node);
  spry_interaction_t* slot = sp_alloc_type(ui->mem, spry_interaction_t);
  *slot = on;
  switch (n->kind) {
    case SPRY_NODE_KIND_BOX:    n->as.box.on = slot;    return;
    case SPRY_NODE_KIND_BUTTON: n->as.button.on = slot; return;
    case SPRY_NODE_KIND_TEXT:   sp_unreachable_case();
    case SPRY_NODE_KIND_LINK:   sp_unreachable_case();
    case SPRY_NODE_KIND_INPUT:  sp_unreachable_case();
  }
}

void spry_ui_body_arg(spry_ui_t* ui, u32 node, sp_str_t key, sp_str_t value) {
  spry_node_t* n = spry_ui_node_at(ui, node);
  spry_interaction_t* on = SP_NULLPTR;
  switch (n->kind) {
    case SPRY_NODE_KIND_BOX:    on = n->as.box.on;    break;
    case SPRY_NODE_KIND_BUTTON: on = n->as.button.on; break;
    case SPRY_NODE_KIND_TEXT:   break;
    case SPRY_NODE_KIND_LINK:   break;
    case SPRY_NODE_KIND_INPUT:  break;
  }
  SP_ASSERT(on && on->kind == SPRY_INTERACTION_KIND_INVOKE);
  if (!on->as.invoke.body) on->as.invoke.body = sp_da_new(ui->mem, spry_invoke_body_entry_t);
  spry_invoke_body_entry_t entry = { .key = key, .value = value };
  sp_da_push(on->as.invoke.body, entry);
}

void spry_ui_append(spry_ui_t* ui, u32 parent, u32 child) {
  SP_ASSERT(spry_ui_node_at(ui, parent)->kind == SPRY_NODE_KIND_BOX);
  SP_ASSERT(child < sp_da_size(ui->entries));
  sp_da_push(ui->entries[parent].children, child);
}

static spry_node_t spry_ui_materialize(spry_ui_t* ui, u32 ref) {
  spry_node_t node = ui->entries[ref].node;
  if (node.kind == SPRY_NODE_KIND_BOX && sp_da_size(ui->entries[ref].children)) {
    sp_da(spry_node_t) children = sp_da_new(ui->mem, spry_node_t);
    sp_da_for(ui->entries[ref].children, i) {
      spry_node_t child = spry_ui_materialize(ui, ui->entries[ref].children[i]);
      sp_da_push(children, child);
    }
    node.as.box.children = children;
  }
  return node;
}

sp_str_t spry_ui_write(spry_ui_t* ui, u32 root) {
  spry_node_t node = spry_ui_materialize(ui, root);
  return spry_ast_write(ui->mem, &spry_node_type, &node);
}
