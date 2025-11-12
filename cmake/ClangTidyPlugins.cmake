# Minimal stub to provide a build target for clang-tidy plugins.
# In a full setup, this would build shared objects against clang-tidy SDK.

include_guard(GLOBAL)

file(GLOB_RECURSE PYCC_TIDY_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/src/clang-tidy/**/*.cpp)

if(NOT TARGET pycc_tidy)
  add_custom_target(pycc_tidy
    COMMAND ${CMAKE_COMMAND} -E echo "[clang-tidy] Building plugins (stub)"
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/pycc_tidy.built
    BYPRODUCTS ${CMAKE_BINARY_DIR}/pycc_tidy.built
    VERBATIM)
endif()
