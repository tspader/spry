#ifndef SPRY_UI_H
#define SPRY_UI_H

#include "sp.h"
#include "abi/ui.builders.gen.h"

spry_ui_t* spry_ui_new(sp_mem_t mem);
void       spry_ui_id(spry_ui_t* ui, u32 node, sp_str_t id);
void       spry_ui_body_arg(spry_ui_t* ui, u32 node, sp_str_t key, sp_str_t value);
void       spry_ui_append(spry_ui_t* ui, u32 parent, u32 child);
sp_str_t   spry_ui_write(spry_ui_t* ui, u32 root);

#endif
