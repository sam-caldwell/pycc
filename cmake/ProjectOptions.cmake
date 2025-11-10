# Project options and common configuration

# Emit intermediates by default (can be disabled)
option(PYCC_EMIT_LLVM "Emit LLVM IR (.ll) files" ON)
option(PYCC_EMIT_ASM  "Emit assembly (.asm) files" ON)

# Export compile commands for tooling and clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

