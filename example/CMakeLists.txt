set(DEMO_GEN_H ${ROOT}/example/native/demo.endpoints.gen.h)
set(DEMO_GEN_TS ${ROOT}/example/web/endpoints.gen.ts)
add_custom_command(
  OUTPUT ${DEMO_GEN_H} ${DEMO_GEN_TS}
  COMMAND ${SPRY_CLI} gen ${ROOT}/example/ui/endpoints.json --prefix demo --c ${DEMO_GEN_H} --ts ${DEMO_GEN_TS}
  DEPENDS ${SPRY_CLI} ${ROOT}/example/ui/endpoints.json
  VERBATIM)
add_custom_target(demo-gen ALL DEPENDS ${DEMO_GEN_H} ${DEMO_GEN_TS})
add_dependencies(demo-gen cli)

add_executable(app
  ${ROOT}/example/native/main.c
  ${ROOT}/example/native/handlers.c
  ${DEMO_GEN_H})
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
  ${ROOT}/example/native/handlers.c
  ${DEMO_GEN_H})
target_include_directories(rpc-smoke PRIVATE ${ROOT}/example/native)
target_link_libraries(rpc-smoke PRIVATE spry::core PkgConfig::SQLITE3)
set_target_properties(rpc-smoke PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${TEST_OUT})
