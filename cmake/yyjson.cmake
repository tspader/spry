set(YYJSON_DIR ${SOURCE}/yyjson/src)

if(NOT EXISTS ${YYJSON_DIR}/yyjson.c)
  message(FATAL_ERROR "yyjson source missing at ${YYJSON_DIR} (run `make fetch`)")
endif()

add_library(yyjson STATIC ${YYJSON_DIR}/yyjson.c)
target_include_directories(yyjson PUBLIC ${YYJSON_DIR})
target_compile_definitions(yyjson PUBLIC
  YYJSON_DISABLE_WRITER=1
  YYJSON_DISABLE_UTILS=1
  YYJSON_DISABLE_INCR_READER=1)

add_executable(yyjson-smoke ${ROOT}/test/yyjson_smoke.c)
target_link_libraries(yyjson-smoke PRIVATE yyjson)
set_target_properties(yyjson-smoke PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${TEST_OUT})

set(YYJSON_DEFS -DYYJSON_DISABLE_WRITER=1 -DYYJSON_DISABLE_UTILS=1 -DYYJSON_DISABLE_INCR_READER=1)
set(YYJSON_PROBE_WASM ${TEST_OUT}/yyjson-probe.wasm)
set(_yw ${CMAKE_BINARY_DIR}/yyjson-wasm)
add_custom_command(
  OUTPUT ${YYJSON_PROBE_WASM}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${_yw} ${TEST_OUT}
  COMMAND zig cc --target=wasm32-wasi -O2 ${YYJSON_DEFS} -I${YYJSON_DIR}
          -c ${YYJSON_DIR}/yyjson.c -o ${_yw}/yyjson.o
  COMMAND zig cc --target=wasm32-wasi -O2 ${YYJSON_DEFS} -I${YYJSON_DIR}
          -c ${ROOT}/test/yyjson_probe.c -o ${_yw}/probe.o
  COMMAND zig cc --target=wasm32-freestanding -O2
          -c ${ROOT}/src/runtime/wasm_shims.c -o ${_yw}/shims.o
  COMMAND zig cc --target=wasm32-freestanding -nostdlib -Wl,--no-entry,--export-dynamic
          -o ${YYJSON_PROBE_WASM} ${_yw}/probe.o ${_yw}/yyjson.o ${_yw}/shims.o
  DEPENDS ${YYJSON_DIR}/yyjson.c ${ROOT}/test/yyjson_probe.c ${ROOT}/src/runtime/wasm_shims.c
  VERBATIM)
add_custom_target(yyjson-wasm ALL DEPENDS ${YYJSON_PROBE_WASM})
