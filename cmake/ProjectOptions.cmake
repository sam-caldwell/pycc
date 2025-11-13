include_guard(GLOBAL)

option(PYCC_EMIT_LLVM "Emit LLVM IR files (.ll)" ON)
option(PYCC_EMIT_ASM  "Emit Assembly files (.asm)" ON)
option(PYCC_ENABLE_TIDY "Enable clang-tidy linting" OFF)
option(PYCC_REQUIRE_TIDY_PLUGIN "Require building custom clang-tidy plugin" OFF)
option(PYCC_COVERAGE "Enable coverage instrumentation (clang)" OFF)
option(PYCC_USE_OPAQUE_PTR_GEP "Use opaque-pointer GEP style in IR (for newer LLVM)" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

if(PYCC_COVERAGE)
  message(STATUS "Coverage instrumentation enabled")
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate -fcoverage-mapping)
  elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(--coverage)
    add_link_options(--coverage)
  endif()
endif()
