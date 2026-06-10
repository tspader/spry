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

set(JTD_SCHEMAS ${ROOT}/src/abi/ui.jtd.json ${ROOT}/src/abi/fault.jtd.json ${ROOT}/src/abi/endpoints.jtd.json)
set(AST_GEN "")
foreach(name ui fault endpoints)
  list(APPEND AST_GEN ${ROOT}/src/abi/${name}.gen.h ${ROOT}/src/abi/${name}.enums.gen.h ${ROOT}/src/abi/${name}.enums.gen.ts)
endforeach()

add_custom_command(
  OUTPUT ${AST_GEN}
  COMMAND bun run ${ROOT}/tools/gen-ast.ts
  DEPENDS ${JTD_SCHEMAS} ${ROOT}/tools/gen-ast.ts ${ROOT}/include/spry/ast.h
  WORKING_DIRECTORY ${ROOT}
  VERBATIM)
add_custom_target(ast DEPENDS ${ROOT}/src/abi/ui.gen.h)

add_custom_command(
  OUTPUT ${ROOT}/src/abi/schema.ts
  COMMAND bun run ${ROOT}/tools/gen-zod.ts
  DEPENDS ${JTD_SCHEMAS} ${ROOT}/tools/gen-zod.ts
  WORKING_DIRECTORY ${ROOT}
  VERBATIM)
add_custom_target(zod DEPENDS ${ROOT}/src/abi/schema.ts)

set(_rt ${CMAKE_BINARY_DIR}/runtime)
set(RUNTIME_WASM ${_rt}/runtime.wasm)
add_custom_command(
  OUTPUT ${RUNTIME_WASM}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${_rt}
  COMMAND zig cc --target=wasm32-freestanding -fno-sanitize=undefined -DSP_ASSERT_LEVEL=SP_ASSERT_TRAP -DYYJSON_FREESTANDING ${YYJSON_DEFS}
          ${INC} -I${ROOT}/src -I${YYJSON_DIR}
          -c ${ROOT}/src/runtime/runtime.c -o ${_rt}/runtime.o
  COMMAND zig cc --target=wasm32-freestanding -fno-sanitize=undefined -DSP_ASSERT_LEVEL=SP_ASSERT_TRAP -DYYJSON_FREESTANDING ${YYJSON_DEFS}
          ${INC} -I${YYJSON_DIR}
          -c ${ROOT}/src/ast/ast.c -o ${_rt}/ast.o
  COMMAND zig cc --target=wasm32-freestanding -O2 -DYYJSON_FREESTANDING ${YYJSON_DEFS} -I${YYJSON_DIR}
          -c ${YYJSON_DIR}/yyjson.c -o ${_rt}/yyjson.o
  COMMAND zig cc --target=wasm32-freestanding -nostdlib -Wl,--no-entry,--export-dynamic
          -o ${RUNTIME_WASM} ${_rt}/runtime.o ${_rt}/ast.o ${_rt}/yyjson.o
  DEPENDS ${ROOT}/src/runtime/runtime.c ${ROOT}/src/ast/ast.c
          ${AST_GEN} ${YYJSON_DIR}/yyjson.c ${HEADERS}
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
          ${ROOT}/src/abi/abi.gen.ts ${ROOT}/src/abi/ui.enums.gen.ts
          ${RUNTIME_WEB} ${ROOT}/tools/bundle-web.ts
  WORKING_DIRECTORY ${ROOT}
  VERBATIM)
add_custom_target(web ALL DEPENDS ${WEB_OUT}/host.bundle.js)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)

add_executable(app
  ${ROOT}/src/host/native/wasm_host.c
  ${ROOT}/src/backend/gtk/gtk_backend.c
  ${ROOT}/src/rpc/rpc.c
  ${ROOT}/example/native/handlers.c
  ${ROOT}/example/native/main.c)
target_include_directories(app PRIVATE ${ROOT}/include ${ROOT}/vendor ${ROOT}/src ${ROOT}/example/native)
target_link_libraries(app PRIVATE PkgConfig::GTK4 PkgConfig::SQLITE3 wasmer yyjson)
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

add_executable(rpc-smoke
  ${ROOT}/src/rpc/rpc.c
  ${ROOT}/example/native/handlers.c
  ${ROOT}/example/native/rpc_smoke.c)
target_include_directories(rpc-smoke PRIVATE ${ROOT}/include ${ROOT}/vendor ${ROOT}/example/native)
target_link_libraries(rpc-smoke PRIVATE PkgConfig::SQLITE3 yyjson)
set_target_properties(rpc-smoke PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${TEST_OUT})

add_executable(ast-tests
  ${ROOT}/test/ast/main.c
  ${ROOT}/src/ast/ast.c)
target_include_directories(ast-tests PRIVATE
  ${ROOT}/include ${ROOT}/vendor ${ROOT}/src ${ROOT}/test)
target_link_libraries(ast-tests PRIVATE yyjson)
add_dependencies(ast-tests ast)
set_target_properties(ast-tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${TEST_OUT})
