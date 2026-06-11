set(DEMO_GEN_H ${ROOT}/example/native/demo.endpoints.gen.h)
set(DEMO_GEN_TS ${ROOT}/example/web/endpoints.gen.ts)
add_custom_command(
  OUTPUT ${DEMO_GEN_H} ${DEMO_GEN_TS}
  COMMAND ${SPRY_CLI} gen ${ROOT}/example/ui/endpoints.json --prefix demo --c ${DEMO_GEN_H} --ts ${DEMO_GEN_TS}
  DEPENDS ${SPRY_CLI} ${ROOT}/example/ui/endpoints.json
  VERBATIM)
add_custom_target(example-codegen ALL DEPENDS ${DEMO_GEN_H} ${DEMO_GEN_TS})
add_dependencies(example-codegen cli)

add_subdirectory(native)
