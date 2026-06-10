set(YYJSON_DIR ${SOURCE}/yyjson/src)

if(NOT EXISTS ${YYJSON_DIR}/yyjson.c)
  message(FATAL_ERROR "yyjson source missing at ${YYJSON_DIR} (run `make fetch`)")
endif()

add_library(yyjson STATIC ${YYJSON_DIR}/yyjson.c)
target_include_directories(yyjson PUBLIC ${YYJSON_DIR})
target_compile_definitions(yyjson PUBLIC
  YYJSON_DISABLE_UTILS=1
  YYJSON_DISABLE_INCR_READER=1)

set(YYJSON_DEFS -DYYJSON_DISABLE_WRITER=1 -DYYJSON_DISABLE_UTILS=1 -DYYJSON_DISABLE_INCR_READER=1)
