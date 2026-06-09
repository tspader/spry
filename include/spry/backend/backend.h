#ifndef BACKEND_H
#define BACKEND_H

#include "sp.h"

typedef struct backend backend_t;

typedef struct {
  void* ctx;
  void (*dispatch)(void* ctx, u32 token);
  void (*deliver)(void* ctx, u32 token, sp_str_t json);
} host_iface_t;

struct backend {
  void* self;
  u32   (*capabilities)(void* self);
  void* (*create_element)(void* self, u32 kind);
  void  (*set_direction)(void* self, void* node, u32 value);
  void  (*set_gap)(void* self, void* node, s32 value);
  void  (*set_padding)(void* self, void* node, s32 value);
  void  (*set_align)(void* self, void* node, u32 value);
  void  (*set_justify)(void* self, void* node, u32 value);
  void  (*set_text)(void* self, void* node, sp_str_t value);
  void  (*set_href)(void* self, void* node, sp_str_t value);
  void  (*set_value)(void* self, void* node, sp_str_t value);
  void  (*set_name)(void* self, void* node, sp_str_t value);
  void  (*set_placeholder)(void* self, void* node, sp_str_t value);
  void  (*append_child)(void* self, void* parent, void* child);
  void  (*set_root)(void* self, void* node);
  void  (*on_event)(void* self, void* node, u32 event, u32 token);
  void  (*submit)(void* self, u32 token, sp_str_t action, sp_str_t body);
  void  (*clear_children)(void* self, void* node);
  u32   (*get_value)(void* self, void* node, c8* out, u32 cap);
  void  (*fatal)(void* self, sp_str_t message);
};

#endif
