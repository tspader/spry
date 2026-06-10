#ifndef DEMO_HANDLERS_H
#define DEMO_HANDLERS_H

#include <sqlite3.h>
#include "sp.h"
#include "spry/rpc.h"

typedef struct {
  sp_mem_t mem;
  sqlite3* db;
  spry_rpc_t* rpc;
} demo_ctx_t;

bool        demo_db_seed(sqlite3* db);
demo_ctx_t* demo_new(sp_mem_t mem, sqlite3* db, spry_endpoints_t endpoints);

#endif
