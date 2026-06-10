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
