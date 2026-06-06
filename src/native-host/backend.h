#ifndef BACKEND_H
#define BACKEND_H

#include "sp.h"

typedef struct backend backend_t;

struct backend {
  void* self;
  u32   (*capabilities)(void* self);
  void* (*create_element)(void* self, u32 kind);
  void  (*set_attr)(void* self, void* node, u32 attr, s32 value);
  void  (*set_attr_str)(void* self, void* node, u32 attr, sp_str_t value);
  void  (*append_child)(void* self, void* parent, void* child);
  void  (*set_root)(void* self, void* node);
  void  (*fatal)(void* self, sp_str_t message);
};

#endif
