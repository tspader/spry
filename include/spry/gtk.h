#ifndef SPRY_GTK_H
#define SPRY_GTK_H

#include "sp.h"
#include "spry/rpc.h"

typedef struct {
  sp_str_t title;
  u32 width;
  u32 height;
  sp_str_t tree;
  sp_str_t endpoints;
  spry_rpc_t* rpc;
  sp_str_t wasm_path;
} spry_gtk_opts_t;

s32 spry_gtk_run(const spry_gtk_opts_t* opts);

#endif
