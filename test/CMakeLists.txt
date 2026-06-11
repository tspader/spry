add_executable(ast-tests
  ${ROOT}/test/ast/main.c
  ${ROOT}/src/ast/ast.c)
target_include_directories(ast-tests PRIVATE
  ${ROOT}/include ${ROOT}/vendor ${ROOT}/src ${ROOT}/test)
target_link_libraries(ast-tests PRIVATE yyjson)
add_dependencies(ast-tests ast)
set_target_properties(ast-tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${TEST_OUT})

add_executable(gen-tests ${ROOT}/test/gen/main.c)
target_include_directories(gen-tests PRIVATE ${ROOT}/test)
target_compile_definitions(gen-tests PRIVATE SPRY_GEN_CASES="${ROOT}/test/gen/cases")
target_link_libraries(gen-tests PRIVATE spry::gen)
set_target_properties(gen-tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${TEST_OUT})

set(UI_CONFORMANCE ${TEST_OUT}/ui-conformance.json)
add_custom_command(
  OUTPUT ${UI_CONFORMANCE}
  COMMAND bun run ${ROOT}/test/ui/conformance.tsx ${UI_CONFORMANCE}
  DEPENDS ${ROOT}/test/ui/conformance.tsx ${ROOT}/src/jsx/jsx-runtime.ts ${ROOT}/src/jsx/jsx-dev-runtime.ts
          ${ROOT}/src/abi/jsx.gen.ts ${ROOT}/src/abi/ui.gen.ts ${ROOT}/src/abi/schema.ts
  WORKING_DIRECTORY ${ROOT}
  VERBATIM)
add_custom_target(ui-conformance ALL DEPENDS ${UI_CONFORMANCE})

add_executable(ui-tests ${ROOT}/test/ui/main.c)
target_include_directories(ui-tests PRIVATE ${ROOT}/test)
target_compile_definitions(ui-tests PRIVATE SPRY_UI_CONFORMANCE="${UI_CONFORMANCE}")
target_link_libraries(ui-tests PRIVATE spry::core)
add_dependencies(ui-tests ui-conformance)
set_target_properties(ui-tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${TEST_OUT})
