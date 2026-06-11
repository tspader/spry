#ifndef SPRY_UI_H
#define SPRY_UI_H

#include "sp.h"
#include "abi/ui.builders.gen.h"

spry_ui_t* spry_ui_new(sp_mem_t mem);
void       spry_ui_body_arg(spry_ui_t* ui, u32 node, sp_str_t key, sp_str_t value);
void       spry_ui_append(spry_ui_t* ui, u32 parent, u32 child);
void       spry_ui_leave(spry_ui_t* ui);
u32        spry_ui_current(spry_ui_t* ui);
u32        spry_ui_root(spry_ui_t* ui);
sp_str_t   spry_ui_fmt(spry_ui_t* ui, const c8* fmt, ...);
sp_str_t   spry_ui_write(spry_ui_t* ui, u32 root);

static inline sp_str_t spry_ui_str_pass(sp_str_t s) {
  return s;
}

#define spry_ui_str(s) _Generic((s), char*: sp_cstr_as_str, const char*: sp_cstr_as_str, sp_str_t: spry_ui_str_pass)(s)

#define SPRY_UI(ui) for (spry_ui_t* spry_ui__ctx = (ui); spry_ui__ctx; spry_ui__ctx = SP_NULLPTR)

#define SPRY_UI_ELEMENT(open_expr) \
  for (s32 spry_ui__latch = ((void)(open_expr), 0); spry_ui__latch < 1; spry_ui__latch = 1, spry_ui_leave(spry_ui__ctx))

#define SPRY_ARG(key, value) spry_ui_body_arg(spry_ui__ctx, spry_ui_current(spry_ui__ctx), spry_ui_str(key), spry_ui_str(value))

#endif
