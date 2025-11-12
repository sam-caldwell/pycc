include_guard(GLOBAL)

add_library(pycc_core ${PYCC_SOURCES} ${PYCC_HEADERS})
target_include_directories(pycc_core PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_executable(pycc ${PYCC_MAIN})
target_link_libraries(pycc PRIVATE pycc_core)
target_include_directories(pycc PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(PYCC_ENABLE_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if(CLANG_TIDY_EXE)
    set_target_properties(pycc PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    set_target_properties(pycc_core PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  endif()
endif()

## Note: The real 'tidy' target is defined in TidyTarget.cmake.
## Remove the placeholder here to avoid shadowing the actual linter.
