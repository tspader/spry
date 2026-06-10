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
set(ASSETS       ${ROOT}/src/assets)
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
set(AST_GEN ${ROOT}/src/abi/ui.builders.gen.h)
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
  OUTPUT ${ROOT}/src/abi/schema.ts ${ROOT}/src/abi/ui.gen.ts
  COMMAND bun run ${ROOT}/tools/gen-types.ts
  DEPENDS ${JTD_SCHEMAS} ${ROOT}/tools/gen-types.ts
  WORKING_DIRECTORY ${ROOT}
  VERBATIM)
add_custom_target(types ALL DEPENDS ${ROOT}/src/abi/schema.ts)

set(WASM_CC zig cc --target=wasm32-freestanding -fno-sanitize=undefined -DSP_ASSERT_LEVEL=SP_ASSERT_TRAP -DYYJSON_FREESTANDING ${YYJSON_DEFS} ${INC} -I${ROOT}/src -I${YYJSON_DIR})

set(_rt ${CMAKE_BINARY_DIR}/runtime)
set(RUNTIME_WASM ${_rt}/runtime.wasm)
add_custom_command(
  OUTPUT ${RUNTIME_WASM}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${_rt}
  COMMAND ${WASM_CC} -c ${ROOT}/src/runtime/runtime.c -o ${_rt}/runtime.o
  COMMAND ${WASM_CC} -c ${ROOT}/src/core/core.c -o ${_rt}/core.o
  COMMAND ${WASM_CC} -c ${ROOT}/src/ast/ast.c -o ${_rt}/ast.o
  COMMAND zig cc --target=wasm32-freestanding -O2 -DYYJSON_FREESTANDING ${YYJSON_DEFS} -I${YYJSON_DIR}
          -c ${YYJSON_DIR}/yyjson.c -o ${_rt}/yyjson.o
  COMMAND zig cc --target=wasm32-freestanding -nostdlib -Wl,--no-entry,--export-dynamic
          -o ${RUNTIME_WASM} ${_rt}/runtime.o ${_rt}/core.o ${_rt}/ast.o ${_rt}/yyjson.o
  DEPENDS ${ROOT}/src/runtime/runtime.c ${ROOT}/src/core/core.c ${ROOT}/src/ast/ast.c
          ${AST_GEN} ${YYJSON_DIR}/yyjson.c ${HEADERS}
  VERBATIM)

set(_svc ${CMAKE_BINARY_DIR}/service)
set(SERVICE_WASM ${_svc}/service.wasm)
add_custom_command(
  OUTPUT ${SERVICE_WASM}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${_svc}
  COMMAND ${WASM_CC} -c ${ROOT}/src/service/service.c -o ${_svc}/service.o
  COMMAND ${WASM_CC} -c ${ROOT}/src/core/core.c -o ${_svc}/core.o
  COMMAND ${WASM_CC} -c ${ROOT}/src/ast/ast.c -o ${_svc}/ast.o
  COMMAND zig cc --target=wasm32-freestanding -O2 -DYYJSON_FREESTANDING ${YYJSON_DEFS} -I${YYJSON_DIR}
          -c ${YYJSON_DIR}/yyjson.c -o ${_svc}/yyjson.o
  COMMAND zig cc --target=wasm32-freestanding -nostdlib -Wl,--no-entry,--export-dynamic
          -o ${SERVICE_WASM} ${_svc}/service.o ${_svc}/core.o ${_svc}/ast.o ${_svc}/yyjson.o
  DEPENDS ${ROOT}/src/service/service.c ${ROOT}/src/core/core.c ${ROOT}/src/ast/ast.c
          ${AST_GEN} ${YYJSON_DIR}/yyjson.c ${HEADERS}
  VERBATIM)

set(RUNTIME_WEB  ${ASSETS}/runtime.wasm)
set(RUNTIME_TEST ${TEST_OUT}/runtime.wasm)
foreach(dst ${RUNTIME_WEB} ${RUNTIME_TEST})
  get_filename_component(dstdir ${dst} DIRECTORY)
  add_custom_command(
    OUTPUT ${dst}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${dstdir}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RUNTIME_WASM} ${dst}
    DEPENDS ${RUNTIME_WASM}
    VERBATIM)
endforeach()

set(SERVICE_ASSET ${ASSETS}/service.wasm)
add_custom_command(
  OUTPUT ${SERVICE_ASSET}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${ASSETS}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SERVICE_WASM} ${SERVICE_ASSET}
  DEPENDS ${SERVICE_WASM}
  VERBATIM)

add_custom_target(runtime ALL DEPENDS ${RUNTIME_WEB} ${RUNTIME_TEST} ${SERVICE_ASSET})

add_custom_command(
  OUTPUT ${ASSETS}/host.bundle.js
  COMMAND bun run ${ROOT}/tools/bundle-web.ts
  DEPENDS ${ROOT}/src/client/main.ts ${ROOT}/src/client/index.html
          ${ROOT}/src/host/web/host.ts ${ROOT}/src/backend/backend.ts ${ROOT}/src/backend/web/dom-backend.ts
          ${ROOT}/src/abi/abi.gen.ts ${ROOT}/src/abi/ui.enums.gen.ts
          ${RUNTIME_WEB} ${ROOT}/tools/bundle-web.ts
  WORKING_DIRECTORY ${ROOT}
  VERBATIM)
add_custom_target(web ALL DEPENDS ${ASSETS}/host.bundle.js)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)

add_library(spry_core STATIC
  ${ROOT}/src/core/sp.c
  ${ROOT}/src/core/core.c
  ${ROOT}/src/core/ui.c
  ${ROOT}/src/rpc/rpc.c
  ${ROOT}/src/ast/ast.c)
target_include_directories(spry_core PUBLIC ${ROOT}/include ${ROOT}/vendor ${ROOT}/src)
target_link_libraries(spry_core PUBLIC yyjson)
add_dependencies(spry_core ast)
add_library(spry::core ALIAS spry_core)

set(EMBED_C ${CMAKE_BINARY_DIR}/embed/runtime_wasm.gen.c)
add_custom_command(
  OUTPUT ${EMBED_C}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/embed
  COMMAND bun run ${ROOT}/tools/embed-wasm.ts ${RUNTIME_WASM} ${EMBED_C} spry_runtime_wasm
  DEPENDS ${RUNTIME_WASM} ${ROOT}/tools/embed-wasm.ts
  VERBATIM)

add_library(spry_host STATIC
  ${ROOT}/src/host/native/wasm_host.c
  ${EMBED_C})
target_link_libraries(spry_host PUBLIC spry_core wasmer)
add_library(spry::host ALIAS spry_host)

add_library(spry_gtk STATIC
  ${ROOT}/src/backend/gtk/gtk_backend.c
  ${ROOT}/src/app/gtk_app.c)
target_link_libraries(spry_gtk PUBLIC spry_host PkgConfig::GTK4)
add_library(spry::gtk ALIAS spry_gtk)

add_library(spry_stub STATIC ${ROOT}/src/backend/stub/stub_backend.c)
target_link_libraries(spry_stub PUBLIC spry_core)
add_library(spry::stub ALIAS spry_stub)

add_executable(app
  ${ROOT}/example/native/main.c
  ${ROOT}/example/native/handlers.c)
target_include_directories(app PRIVATE ${ROOT}/example/native)
target_link_libraries(app PRIVATE spry::gtk PkgConfig::SQLITE3)
set_target_properties(app PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${STORE_TARGET}/gtk)

add_executable(native-smoke ${ROOT}/example/native/smoke.c)
target_link_libraries(native-smoke PRIVATE spry::host spry::stub)
set_target_properties(native-smoke PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${TEST_OUT})
add_dependencies(native-smoke runtime)

add_executable(rpc-smoke
  ${ROOT}/example/native/rpc_smoke.c
  ${ROOT}/example/native/handlers.c)
target_include_directories(rpc-smoke PRIVATE ${ROOT}/example/native)
target_link_libraries(rpc-smoke PRIVATE spry::core PkgConfig::SQLITE3)
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
