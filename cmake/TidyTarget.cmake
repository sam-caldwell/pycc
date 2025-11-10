# (c) 2025 Sam Caldwell. See LICENSE.txt
# cmake/TidyTarget.cmake

if(NOT DEFINED CLANG_TIDY_EXE)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
endif()

set(TIDY_CONFIG_FILE "${CMAKE_SOURCE_DIR}/.clang-tidy")

# Collect project sources to lint (exclude build*/ and external deps).
set(PYCC_PLUGIN_SOURCES)
if(PYCC_HAVE_CLANG_ASTMATCHERS)
  file(GLOB_RECURSE PYCC_PLUGIN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/clang-tidy/*.cpp")
endif()

set(TIDY_FILES
  ${PYCC_DRIVER_SOURCES}
  "${CMAKE_SOURCE_DIR}/src/main.cpp"
  ${PYCC_PLUGIN_SOURCES}
)

list(REMOVE_DUPLICATES TIDY_FILES)

set(TIDY_ARGS "--quiet" "--extra-arg-before=--driver-mode=c++" "--config-file=${TIDY_CONFIG_FILE}" "-p" "${CMAKE_BINARY_DIR}")
if(DEFINED PYCC_TIDY_PLUGIN_PATH AND EXISTS "${PYCC_TIDY_PLUGIN_PATH}")
  list(INSERT TIDY_ARGS 0 "--load=${PYCC_TIDY_PLUGIN_PATH}")
endif()

# Help clang-tidy find standard library headers on macOS (Homebrew toolchains).
if(APPLE)
  set(_PYCC_SYSROOT "")
  if(DEFINED CMAKE_OSX_SYSROOT AND NOT CMAKE_OSX_SYSROOT STREQUAL "")
    set(_PYCC_SYSROOT "${CMAKE_OSX_SYSROOT}")
  else()
    execute_process(
      COMMAND xcrun --show-sdk-path
      OUTPUT_VARIABLE _PYCC_SYSROOT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
  endif()
  if(_PYCC_SYSROOT)
    list(APPEND TIDY_ARGS "--extra-arg-before=-isysroot" "--extra-arg-before=${_PYCC_SYSROOT}" "--extra-arg=-stdlib=libc++")
  endif()
endif()

if(CLANG_TIDY_EXE)
  add_custom_target(tidy
    COMMAND ${CLANG_TIDY_EXE} ${TIDY_ARGS} ${TIDY_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-tidy on project sources")
  if(TARGET pycc_tidy)
    add_dependencies(tidy pycc_tidy)
  endif()
else()
  add_custom_target(tidy
    COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found; skipping"
    COMMENT "clang-tidy not found; skipping")
endif()
