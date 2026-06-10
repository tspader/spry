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
set(SPRY_CLI     ${STORE_TARGET}/bin/spry)

set(INC -I${ROOT}/include -I${ROOT}/vendor)

file(GLOB_RECURSE HEADERS ${ROOT}/include/*.h ${ROOT}/src/*.h ${ROOT}/vendor/*.h)

include(${ROOT}/cmake/yyjson.cmake)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)

add_subdirectory(src)
add_subdirectory(example)
add_subdirectory(test)
