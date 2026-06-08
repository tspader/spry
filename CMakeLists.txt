cmake_minimum_required(VERSION 3.20)

set(CMAKE_C_COMPILER "${CMAKE_CURRENT_LIST_DIR}/cmake/zig-cc.sh" CACHE FILEPATH "")

project(spry C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_LINK_DEPENDS_USE_LINKER FALSE)

if(NOT DEFINED TRIPLE)
  set(TRIPLE "${CMAKE_SYSTEM_PROCESSOR}-linux-gnu")
endif()

set(ROOT         ${CMAKE_SOURCE_DIR})
set(SOURCE       ${ROOT}/.build/source)
set(STORE_ROOT   ${ROOT}/.build/store)
set(STORE_TARGET ${STORE_ROOT}/${TRIPLE})
set(WEB_OUT      ${STORE_ROOT}/web/example)
set(TEST_OUT     ${STORE_ROOT}/test)

set(INC -I${ROOT}/include -I${ROOT}/vendor)

file(GLOB_RECURSE HEADERS ${ROOT}/include/*.h ${ROOT}/src/*.h ${ROOT}/vendor/*.h)

include(${ROOT}/cmake/yyjson.cmake)

add_custom_command(
  OUTPUT ${ROOT}/src/abi/abi.gen.ts
  COMMAND bun run ${ROOT}/tools/gen-abi.ts
  DEPENDS ${ROOT}/include/spry/abi.h ${ROOT}/tools/gen-abi.ts
  WORKING_DIRECTORY ${ROOT}
  VERBATIM)
add_custom_target(abi DEPENDS ${ROOT}/src/abi/abi.gen.ts)

set(_rt ${CMAKE_BINARY_DIR}/runtime)
set(RUNTIME_WASM ${_rt}/runtime.wasm)
add_custom_command(
  OUTPUT ${RUNTIME_WASM}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${_rt}
  COMMAND zig cc --target=wasm32-freestanding -fno-sanitize=undefined ${YYJSON_DEFS}
          ${INC} -I${ROOT}/src/runtime/wasm_libc -I${YYJSON_DIR}
          -c ${ROOT}/src/runtime/runtime.c -o ${_rt}/runtime.o
  COMMAND zig cc --target=wasm32-wasi -O2 ${YYJSON_DEFS} -I${YYJSON_DIR}
          -c ${YYJSON_DIR}/yyjson.c -o ${_rt}/yyjson.o
  COMMAND zig cc --target=wasm32-freestanding -O2
          -c ${ROOT}/src/runtime/wasm_shims.c -o ${_rt}/shims.o
  COMMAND zig cc --target=wasm32-freestanding -nostdlib -Wl,--no-entry,--export-dynamic
          -o ${RUNTIME_WASM} ${_rt}/runtime.o ${_rt}/yyjson.o ${_rt}/shims.o
  DEPENDS ${ROOT}/src/runtime/runtime.c ${ROOT}/src/runtime/wasm_shims.c
          ${YYJSON_DIR}/yyjson.c ${HEADERS}
  VERBATIM)

set(RUNTIME_WEB  ${WEB_OUT}/runtime.wasm)
set(RUNTIME_GTK  ${STORE_TARGET}/gtk/runtime.wasm)
set(RUNTIME_TEST ${TEST_OUT}/runtime.wasm)
foreach(dst ${RUNTIME_WEB} ${RUNTIME_GTK} ${RUNTIME_TEST})
  get_filename_component(dstdir ${dst} DIRECTORY)
  add_custom_command(
    OUTPUT ${dst}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${dstdir}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RUNTIME_WASM} ${dst}
    DEPENDS ${RUNTIME_WASM}
    VERBATIM)
endforeach()
add_custom_target(runtime ALL DEPENDS ${RUNTIME_WEB} ${RUNTIME_GTK} ${RUNTIME_TEST})

add_custom_command(
  OUTPUT ${WEB_OUT}/host.bundle.js
  COMMAND bun run ${ROOT}/tools/bundle-web.ts
  DEPENDS ${ROOT}/example/web/main.ts ${ROOT}/example/web/index.html
          ${ROOT}/src/abi/abi.gen.ts ${RUNTIME_WEB} ${ROOT}/tools/bundle-web.ts
  WORKING_DIRECTORY ${ROOT}
  VERBATIM)
add_custom_target(web ALL DEPENDS ${WEB_OUT}/host.bundle.js)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)

add_executable(app
  ${ROOT}/src/host/native/wasm_host.c
  ${ROOT}/src/backend/gtk/gtk_backend.c
  ${ROOT}/example/native/main.c)
target_include_directories(app PRIVATE ${ROOT}/include ${ROOT}/vendor)
target_link_libraries(app PRIVATE PkgConfig::GTK4 wasmer)
set_target_properties(app PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${STORE_TARGET}/gtk)
add_dependencies(app runtime)

add_executable(native-smoke
  ${ROOT}/src/host/native/wasm_host.c
  ${ROOT}/src/backend/stub/stub_backend.c
  ${ROOT}/example/native/smoke.c)
target_include_directories(native-smoke PRIVATE ${ROOT}/include ${ROOT}/vendor)
target_link_libraries(native-smoke PRIVATE wasmer)
set_target_properties(native-smoke PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${TEST_OUT})
add_dependencies(native-smoke runtime)
