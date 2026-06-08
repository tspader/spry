#include "yyjson.h"

static unsigned char pool[65536];

__attribute__((export_name("probe")))
int probe(void) {
  yyjson_alc alc;
  if (!yyjson_alc_pool_init(&alc, pool, sizeof(pool))) return -1;

  const char* json = "[1,2,3,4,5]";
  yyjson_doc* doc = yyjson_read_opts((char*)json, 11, 0, &alc, 0);
  if (!doc) return -2;

  int n = (int)yyjson_arr_size(yyjson_doc_get_root(doc));
  yyjson_doc_free(doc);
  return n;
}
