# Optional build of pycc custom clang-tidy plugins

option(PYCC_BUILD_TIDY_PLUGINS "Build pycc clang-tidy plugins" OFF)

if(PYCC_BUILD_TIDY_PLUGINS)
  # Try to locate Clang config. If not found, derive LLVM root from the clang binary on PATH.
  find_package(Clang QUIET CONFIG)
  if(NOT Clang_FOUND)
    # Discover clang binary location (Homebrew installs in /opt/homebrew/opt/llvm/bin)
    find_program(_PYCC_CLANG_PROG NAMES clang clang-19 clang-18 clang-17)
    if(_PYCC_CLANG_PROG)
      get_filename_component(_PYCC_CLANG_BIN_DIR "${_PYCC_CLANG_PROG}" DIRECTORY)
      get_filename_component(_PYCC_LLVM_ROOT "${_PYCC_CLANG_BIN_DIR}/.." REALPATH)
      set(_PYCC_LLVM_CMAKE_DIR "${_PYCC_LLVM_ROOT}/lib/cmake")
      if(EXISTS "${_PYCC_LLVM_CMAKE_DIR}")
        # Append locally so we don't require persistent environment variables.
        list(APPEND CMAKE_PREFIX_PATH "${_PYCC_LLVM_CMAKE_DIR}")
      endif()
      # Retry discovery now that we've hinted the prefix path.
      find_package(Clang QUIET CONFIG)
    endif()
    # Homebrew fallback paths if clang isn't in PATH
    if(NOT Clang_FOUND)
      foreach(_brew_root IN LISTS _PYCC_LLVM_ROOT)
      endforeach()
      if(EXISTS "/opt/homebrew/opt/llvm/lib/cmake")
        list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/llvm/lib/cmake")
        find_package(Clang QUIET CONFIG)
      elseif(EXISTS "/usr/local/opt/llvm/lib/cmake")
        list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/llvm/lib/cmake")
        find_package(Clang QUIET CONFIG)
      endif()
    endif()
  endif()
  if(NOT Clang_FOUND)
    message(STATUS "Clang (with tidy) not found; skipping plugin build")
  else()
    # Try to locate ASTMatchers header; search both Clang-provided include dirs and the discovered LLVM root include.
    set(_PYCC_CLANG_INCLUDE_HINTS ${Clang_INCLUDE_DIRS})
    if(DEFINED _PYCC_LLVM_ROOT)
      list(APPEND _PYCC_CLANG_INCLUDE_HINTS "${_PYCC_LLVM_ROOT}/include")
    endif()
    find_path(CLANG_ASTMATCH_INCLUDE clang/ASTMatchers/ASTMatchFinder.h
      HINTS ${_PYCC_CLANG_INCLUDE_HINTS})
    if(NOT CLANG_ASTMATCH_INCLUDE)
      message(STATUS "Clang ASTMatchers headers not found; skipping plugin build")
      set(PYCC_BUILD_TIDY_PLUGINS OFF CACHE BOOL "" FORCE)
      return()
    endif()
    add_library(pycc_tidy MODULE
      src/clang-tidy/declare-only/RegisterMatchers.cpp
      src/clang-tidy/declare-only/Check.cpp
      src/clang-tidy/one-function-per-file/RegisterMatchers.cpp
      src/clang-tidy/one-function-per-file/Check.cpp
      src/clang-tidy/docstrings-checker/RegisterMatchers.cpp
      src/clang-tidy/docstrings-checker/Check.cpp
      src/clang-tidy/module/RegisterModule.cpp)
    target_include_directories(pycc_tidy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${Clang_INCLUDE_DIRS} ${CLANG_ASTMATCH_INCLUDE})
    target_link_libraries(pycc_tidy PRIVATE clangTidy clangTooling clangAST clangASTMatchers clangBasic clangFrontend)
    set_target_properties(pycc_tidy PROPERTIES
      OUTPUT_NAME "pycc-tidy"
      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    # Expose the absolute plugin path for external tooling (Makefile lint).
    set(PYCC_TIDY_PLUGIN_PATH "${CMAKE_BINARY_DIR}/${CMAKE_SHARED_MODULE_PREFIX}pycc-tidy${CMAKE_SHARED_MODULE_SUFFIX}"
        CACHE STRING "Path to pycc Clang-Tidy plugin" FORCE)
  endif()
endif()
