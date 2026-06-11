#include "spry/ui.h"
#include "spry/ast.h"

typedef struct {
  spry_node_t node;
  sp_da(u32) children;
} spry_ui_entry_t;

struct spry_ui {
  sp_mem_t mem;
  sp_da(spry_ui_entry_t) entries;
  sp_da(u32) open;
  u32 root;
  bool has_root;
};

spry_ui_t* spry_ui_new(sp_mem_t mem) {
  spry_ui_t* ui = sp_alloc_type(mem, spry_ui_t);
  *ui = sp_zero_s(spry_ui_t);
  ui->mem = mem;
  ui->entries = sp_da_new(mem, spry_ui_entry_t);
  ui->open = sp_da_new(mem, u32);
  return ui;
}

static spry_node_t* spry_ui_node_at(spry_ui_t* ui, u32 node) {
  SP_ASSERT(node < sp_da_size(ui->entries));
  return &ui->entries[node].node;
}

static u32 spry_ui_push(spry_ui_t* ui, spry_node_t node) {
  spry_ui_entry_t entry = sp_zero_s(spry_ui_entry_t);
  entry.node = node;
  entry.children = sp_da_new(ui->mem, u32);
  sp_da_push(ui->entries, entry);
  return (u32)(sp_da_size(ui->entries) - 1);
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

void spry_ui_arg_str(spry_ui_t* ui, u32 node, sp_str_t key, sp_str_t value) {
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

void spry_ui_arg_cstr(spry_ui_t* ui, u32 node, sp_str_t key, const c8* value) {
  spry_ui_arg_str(ui, node, key, sp_cstr_as_str(value));
}

void spry_ui_arg_s64(spry_ui_t* ui, u32 node, sp_str_t key, s64 value) {
  spry_ui_arg_str(ui, node, key, spry_ui_fmt(ui, "{}", sp_fmt_int(value)));
}

void spry_ui_arg_u64(spry_ui_t* ui, u32 node, sp_str_t key, u64 value) {
  spry_ui_arg_str(ui, node, key, spry_ui_fmt(ui, "{}", sp_fmt_uint(value)));
}

void spry_ui_arg_f64(spry_ui_t* ui, u32 node, sp_str_t key, f64 value) {
  spry_ui_arg_str(ui, node, key, spry_ui_fmt(ui, "{}", sp_fmt_float(value)));
}

void spry_ui_arg_bool(spry_ui_t* ui, u32 node, sp_str_t key, bool value) {
  spry_ui_arg_str(ui, node, key, value ? sp_str_lit("true") : sp_str_lit("false"));
}

void spry_ui_append(spry_ui_t* ui, u32 parent, u32 child) {
  SP_ASSERT(spry_ui_node_at(ui, parent)->kind == SPRY_NODE_KIND_BOX);
  SP_ASSERT(child < sp_da_size(ui->entries));
  sp_da_push(ui->entries[parent].children, child);
}

u32 spry_ui_enter(spry_ui_t* ui, spry_node_t node) {
  u32 child = spry_ui_push(ui, node);
  if (sp_da_size(ui->open)) {
    spry_ui_append(ui, *sp_da_back(ui->open), child);
  } else {
    SP_ASSERT(!ui->has_root);
    ui->root = child;
    ui->has_root = true;
  }
  sp_da_push(ui->open, child);
  return child;
}

void spry_ui_leave(spry_ui_t* ui) {
  SP_ASSERT(sp_da_size(ui->open));
  sp_da_pop(ui->open);
}

u32 spry_ui_current(spry_ui_t* ui) {
  SP_ASSERT(sp_da_size(ui->open));
  return *sp_da_back(ui->open);
}

u32 spry_ui_root(spry_ui_t* ui) {
  SP_ASSERT(ui->has_root);
  SP_ASSERT(sp_da_size(ui->open) == 0);
  return ui->root;
}

sp_str_t spry_ui_fmt(spry_ui_t* ui, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_t out = sp_fmt_mem_v(ui->mem, sp_cstr_as_str(fmt), args).value;
  va_end(args);
  return out;
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
  SP_ASSERT(sp_da_size(ui->open) == 0);
  spry_node_t node = spry_ui_materialize(ui, root);
  return spry_ast_write(ui->mem, &spry_node_type, &node);
}
