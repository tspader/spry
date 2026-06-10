#ifndef SPRY_GEN_H
#define SPRY_GEN_H

#include "sp.h"
#include "spry/ast.h"

typedef struct {
  sp_str_t prefix;
  sp_str_t endpoints;
} spry_gen_opts_t;

typedef struct {
  spry_err_t code;
  union {
    struct { sp_str_t name; } prefix;
    struct { sp_str_t name; } endpoint;
    struct { sp_str_t endpoint; sp_str_t name; } arg;
    spry_issue_t parse;
    sp_err_t write;
  } as;
} spry_gen_err_t;

spry_gen_err_t spry_gen_c(sp_mem_t mem, spry_gen_opts_t opts, sp_io_writer_t* w);
spry_gen_err_t spry_gen_ts(sp_mem_t mem, spry_gen_opts_t opts, sp_io_writer_t* w);
sp_str_t spry_gen_err_str(sp_mem_t mem, const spry_gen_err_t* error);

#endif
