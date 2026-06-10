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

add_library(spry_gen STATIC ${ROOT}/src/gen/gen.c)
target_link_libraries(spry_gen PUBLIC spry_core)
add_library(spry::gen ALIAS spry_gen)

set(_cli ${CMAKE_BINARY_DIR}/cli)
set(CLI_CC zig cc --target=${CMAKE_SYSTEM_PROCESSOR}-linux-musl -Os ${YYJSON_DEFS} ${INC} -I${ROOT}/src -I${YYJSON_DIR})
add_custom_command(
  OUTPUT ${SPRY_CLI}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${_cli}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${STORE_TARGET}/bin
  COMMAND ${CLI_CC} -c ${ROOT}/src/cli/main.c -o ${_cli}/main.o
  COMMAND ${CLI_CC} -c ${ROOT}/src/gen/gen.c -o ${_cli}/gen.o
  COMMAND ${CLI_CC} -c ${ROOT}/src/core/core.c -o ${_cli}/core.o
  COMMAND ${CLI_CC} -c ${ROOT}/src/ast/ast.c -o ${_cli}/ast.o
  COMMAND ${CLI_CC} -c ${ROOT}/src/core/sp.c -o ${_cli}/sp.o
  COMMAND ${CLI_CC} -O2 -c ${YYJSON_DIR}/yyjson.c -o ${_cli}/yyjson.o
  COMMAND zig cc --target=${CMAKE_SYSTEM_PROCESSOR}-linux-musl -static -o ${SPRY_CLI}
          ${_cli}/main.o ${_cli}/gen.o ${_cli}/core.o ${_cli}/ast.o ${_cli}/sp.o ${_cli}/yyjson.o
  DEPENDS ${ROOT}/src/cli/main.c ${ROOT}/src/gen/gen.c ${ROOT}/src/core/core.c ${ROOT}/src/ast/ast.c
          ${ROOT}/src/core/sp.c ${AST_GEN} ${YYJSON_DIR}/yyjson.c ${HEADERS}
  VERBATIM)
add_custom_target(cli ALL DEPENDS ${SPRY_CLI})
