set(DEMO_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB DEMO_HANDLERS CONFIGURE_DEPENDS ${DEMO_DIR}/handlers/*.c)

add_executable(app
  ${DEMO_DIR}/main.c
  ${DEMO_DIR}/app/app.c
  ${DEMO_HANDLERS}
  ${DEMO_DIR}/db/db.c
  ${DEMO_GEN_H})
target_include_directories(app PRIVATE ${DEMO_DIR} ${DEMO_DIR}/db)
target_link_libraries(app PRIVATE spry::gtk PkgConfig::SQLITE3)
set_target_properties(app PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${STORE_TARGET}/gtk)

add_dependencies(app example-codegen)
