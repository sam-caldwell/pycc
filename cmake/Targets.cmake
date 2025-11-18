include_guard(GLOBAL)

add_library(pycc_core ${PYCC_SOURCES} ${PYCC_HEADERS})
target_include_directories(pycc_core PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Link pthreads for runtime (conservative scanning uses pthread APIs)
find_package(Threads)
if(Threads_FOUND)
  target_link_libraries(pycc_core PRIVATE Threads::Threads)
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
