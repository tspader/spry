set(SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.build/source")

set(DEPS
  "yyjson|https://github.com/ibireme/yyjson.git|1ea2fb0710ceb2198c1044ab885483f228f76761"
)

foreach(dep ${DEPS})
  string(REPLACE "|" ";" parts "${dep}")
  list(GET parts 0 name)
  list(GET parts 1 url)
  list(GET parts 2 commit)

  set(dest "${SOURCE_ROOT}/${name}")

  if(EXISTS "${dest}/.git")
    execute_process(
      COMMAND git -C "${dest}" rev-parse HEAD
      OUTPUT_VARIABLE have OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    if(have STREQUAL commit)
      message(STATUS "fetch: ${name} already at ${commit}")
      continue()
    endif()
  endif()

  message(STATUS "fetch: ${name} -> ${commit}")
  file(MAKE_DIRECTORY "${dest}")
  execute_process(COMMAND git -C "${dest}" init -q)
  execute_process(COMMAND git -C "${dest}" remote remove origin ERROR_QUIET)
  execute_process(COMMAND git -C "${dest}" remote add origin "${url}")
  execute_process(
    COMMAND git -C "${dest}" fetch --depth 1 --quiet origin "${commit}"
    RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "fetch: failed to fetch ${name} ${commit}")
  endif()
  execute_process(COMMAND git -C "${dest}" checkout -q FETCH_HEAD)
endforeach()
