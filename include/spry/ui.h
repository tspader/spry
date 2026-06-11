#ifndef SPRY_UI_H
#define SPRY_UI_H

#include "sp.h"
#include "spry/core.h"
#include "abi/ui.builders.gen.h"

spry_ui_t* spry_ui_new(sp_mem_t mem);
void       spry_ui_append(spry_ui_t* ui, u32 parent, u32 child);
void       spry_ui_leave(spry_ui_t* ui);
u32        spry_ui_current(spry_ui_t* ui);
u32        spry_ui_root(spry_ui_t* ui);
sp_str_t   spry_ui_fmt(spry_ui_t* ui, const c8* fmt, ...);
sp_str_t   spry_ui_write(spry_ui_t* ui, u32 root);

void spry_ui_arg_str(spry_ui_t* ui, u32 node, sp_str_t key, sp_str_t value);
void spry_ui_arg_cstr(spry_ui_t* ui, u32 node, sp_str_t key, const c8* value);
void spry_ui_arg_s64(spry_ui_t* ui, u32 node, sp_str_t key, s64 value);
void spry_ui_arg_u64(spry_ui_t* ui, u32 node, sp_str_t key, u64 value);
void spry_ui_arg_f64(spry_ui_t* ui, u32 node, sp_str_t key, f64 value);
void spry_ui_arg_bool(spry_ui_t* ui, u32 node, sp_str_t key, bool value);

#define SPRY_UI(ui) for (spry_ui_t* spry_ui__ctx = (ui); spry_ui__ctx; spry_ui__ctx = SP_NULLPTR)

#define SPRY_UI_ELEMENT(open_expr) \
  for (s32 spry_ui__latch = ((void)(open_expr), 0); spry_ui__latch < 1; spry_ui__latch = 1, spry_ui_leave(spry_ui__ctx))

#define SPRY_ARG(key, value) \
  _Generic((value), \
    sp_str_t: spry_ui_arg_str, \
    char*: spry_ui_arg_cstr, \
    const char*: spry_ui_arg_cstr, \
    bool: spry_ui_arg_bool, \
    f32: spry_ui_arg_f64, \
    f64: spry_ui_arg_f64, \
    u8: spry_ui_arg_u64, \
    u16: spry_ui_arg_u64, \
    u32: spry_ui_arg_u64, \
    u64: spry_ui_arg_u64, \
    default: spry_ui_arg_s64 \
  )(spry_ui__ctx, spry_ui_current(spry_ui__ctx), spry_str(key), (value))

#endif
