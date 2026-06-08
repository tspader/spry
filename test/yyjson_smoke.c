#include "yyjson.h"
#include <stdio.h>
#include <string.h>

int main(void) {
  const char* json = "{\"name\":\"spry\",\"nums\":[2,3,4]}";
  yyjson_doc* doc = yyjson_read(json, strlen(json), 0);
  if (!doc) {
    fprintf(stderr, "yyjson-smoke: parse failed\n");
    return 1;
  }

  yyjson_val* root = yyjson_doc_get_root(doc);
  yyjson_val* name = yyjson_obj_get(root, "name");
  yyjson_val* nums = yyjson_obj_get(root, "nums");

  const char* name_str = yyjson_get_str(name);
  size_t num_count = yyjson_arr_size(nums);

  int ok = name_str && strcmp(name_str, "spry") == 0 && num_count == 3;
  printf("yyjson-smoke: version=%s name=%s nums=%zu -> %s\n",
         YYJSON_VERSION_STRING, name_str ? name_str : "(null)", num_count,
         ok ? "ok" : "FAIL");

  yyjson_doc_free(doc);
  return ok ? 0 : 1;
}
