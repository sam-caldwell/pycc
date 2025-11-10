# Optional build of pycc custom clang-tidy plugins

option(PYCC_BUILD_TIDY_PLUGINS "Build pycc clang-tidy plugins" OFF)

if(PYCC_BUILD_TIDY_PLUGINS)
  find_package(Clang QUIET CONFIG)
  if(NOT Clang_FOUND)
    message(WARNING "Clang (with tidy) not found; skipping plugin build")
  else()
    add_library(pycc_tidy MODULE
      src/clang-tidy/declare-only/RegisterMatchers.cpp
      src/clang-tidy/declare-only/Check.cpp
      src/clang-tidy/one-function-per-file/RegisterMatchers.cpp
      src/clang-tidy/one-function-per-file/Check.cpp
      src/clang-tidy/docstrings-checker/RegisterMatchers.cpp
      src/clang-tidy/docstrings-checker/Check.cpp
      src/clang-tidy/module/RegisterModule.cpp)
    target_include_directories(pycc_tidy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${Clang_INCLUDE_DIRS})
    target_link_libraries(pycc_tidy PRIVATE clangTidy clangTooling clangAST clangASTMatchers clangBasic clangFrontend)
    set_target_properties(pycc_tidy PROPERTIES
      OUTPUT_NAME "pycc-tidy"
      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    # Expose the absolute plugin path for external tooling (Makefile lint).
    set(PYCC_TIDY_PLUGIN_PATH "${CMAKE_BINARY_DIR}/${CMAKE_SHARED_MODULE_PREFIX}pycc-tidy${CMAKE_SHARED_MODULE_SUFFIX}"
        CACHE STRING "Path to pycc Clang-Tidy plugin" FORCE)
  endif()
endif()
