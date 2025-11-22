include_guard(GLOBAL)

add_library(pycc_core ${PYCC_SOURCES} ${PYCC_HEADERS})
target_include_directories(pycc_core PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Link pthreads for runtime (conservative scanning uses pthread APIs)
find_package(Threads)
if(Threads_FOUND)
  target_link_libraries(pycc_core PRIVATE Threads::Threads)
endif()

# Optional ICU for Unicode normalization/encoding helpers
option(PYCC_WITH_ICU "Enable ICU-based Unicode normalization" OFF)
if(PYCC_WITH_ICU)
  find_package(ICU COMPONENTS uc i18n QUIET)
  if(ICU_FOUND)
    target_link_libraries(pycc_core PRIVATE ICU::uc ICU::i18n)
    target_compile_definitions(pycc_core PRIVATE PYCC_WITH_ICU)
  endif()
endif()

add_executable(pycc ${PYCC_MAIN})
target_link_libraries(pycc PRIVATE pycc_core)
target_include_directories(pycc PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Toggle IR emission style for GEP based on opaque-pointer preference
if(PYCC_USE_OPAQUE_PTR_GEP)
  target_compile_definitions(pycc_core PRIVATE PYCC_USE_OPAQUE_PTR_GEP)
endif()

if(PYCC_ENABLE_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if(CLANG_TIDY_EXE)
    set_target_properties(pycc PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    set_target_properties(pycc_core PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  endif()
endif()

# Optional: build LLVM pass plugin if LLVM is available
if(PYCC_BUILD_LLVM_PASSES)
  find_package(LLVM QUIET CONFIG)
  if(LLVM_FOUND)
    message(STATUS "LLVM found: ${LLVM_PACKAGE_VERSION}")
    message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")
    message(STATUS "LLVM defs: ${LLVM_DEFINITIONS}")
    add_library(pycc_llvm_passes SHARED
      ${CMAKE_SOURCE_DIR}/src/llvm/ElideGCBarrierPass.cpp)
    # Mark LLVM includes as SYSTEM to avoid treating their warnings as errors
    target_include_directories(pycc_llvm_passes SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(pycc_llvm_passes PRIVATE ${LLVM_DEFINITIONS})
    # Link against umbrella LLVM lib if present; fallback to common components
    if(TARGET LLVM)
      target_link_libraries(pycc_llvm_passes PRIVATE LLVM)
    else()
      # Common subset required for pass plugins
      set(_llvm_libs LLVMCore LLVMSupport LLVMPasses LLVMIRReader LLVMAnalysis LLVMTransformUtils)
      foreach(L IN LISTS _llvm_libs)
        if(TARGET ${L})
          target_link_libraries(pycc_llvm_passes PRIVATE ${L})
        endif()
      endforeach()
    endif()
    # Expose plugin full path to the compiler so Codegen can invoke opt with it
    target_compile_definitions(pycc PRIVATE PYCC_LLVM_PASS_PLUGIN_PATH="$<TARGET_FILE:pycc_llvm_passes>")
    target_compile_definitions(pycc_core PRIVATE PYCC_LLVM_PASS_PLUGIN_PATH="$<TARGET_FILE:pycc_llvm_passes>")
  else()
    message(STATUS "LLVM not found; skipping LLVM pass plugin build")
  endif()
endif()

## Note: The real 'tidy' target is defined in TidyTarget.cmake.
## Remove the placeholder here to avoid shadowing the actual linter.

# Runtime-only library for isolated tests
add_library(pycc_runtime
  ${CMAKE_SOURCE_DIR}/src/runtime/Runtime.cpp
  ${CMAKE_SOURCE_DIR}/include/runtime/Runtime.h)
target_include_directories(pycc_runtime PUBLIC ${CMAKE_SOURCE_DIR}/include)
if(Threads_FOUND)
  target_link_libraries(pycc_runtime PRIVATE Threads::Threads)
endif()
if(PYCC_WITH_ICU AND ICU_FOUND)
  target_link_libraries(pycc_runtime PRIVATE ICU::uc ICU::i18n)
  target_compile_definitions(pycc_runtime PRIVATE PYCC_WITH_ICU)
endif()

# Provide absolute path to runtime library for Codegen to link user programs
target_compile_definitions(pycc PRIVATE PYCC_RUNTIME_LIB_PATH="$<TARGET_FILE:pycc_runtime>")
target_compile_definitions(pycc_core PRIVATE PYCC_RUNTIME_LIB_PATH="$<TARGET_FILE:pycc_runtime>")

# GC benchmark utility
add_executable(bench_runtime ${CMAKE_SOURCE_DIR}/tools/bench_gc.cpp)
target_link_libraries(bench_runtime PRIVATE pycc_runtime)
target_include_directories(bench_runtime PRIVATE ${CMAKE_SOURCE_DIR}/include)

# IR dump utility for debugging Codegen::generateIR
add_executable(ir_dump ${CMAKE_SOURCE_DIR}/tools/ir_dump.cpp)
target_link_libraries(ir_dump PRIVATE pycc_core)
target_include_directories(ir_dump PRIVATE ${CMAKE_SOURCE_DIR}/include)
