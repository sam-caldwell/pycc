include_guard(GLOBAL)

# Enable CTest and set a sensible global timeout
include(CTest)
set(CTEST_TEST_TIMEOUT 300)
if(BUILD_TESTING)
  include(FetchContent)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  FetchContent_MakeAvailable(googletest)

  enable_testing()

  # Discover tests following: test/{component}/{unit,integration,e2e}/*.cpp
  file(GLOB TEST_UNIT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/test/*/unit/*.cpp)
  file(GLOB TEST_INTEGRATION_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/test/*/integration/*.cpp)
  file(GLOB TEST_E2E_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/test/*/e2e/*.cpp)

  if(TEST_UNIT_SOURCES)
    add_executable(test_unit ${TEST_UNIT_SOURCES})
    target_link_libraries(test_unit PRIVATE pycc_core GTest::gtest_main)
    target_include_directories(test_unit PRIVATE ${CMAKE_SOURCE_DIR}/include)
    add_test(NAME test_unit COMMAND test_unit --gtest_color=yes --gtest_print_time=1)
    set_tests_properties(test_unit PROPERTIES TIMEOUT 300)
  endif()

  if(TEST_INTEGRATION_SOURCES)
    add_executable(test_integration ${TEST_INTEGRATION_SOURCES})
    target_link_libraries(test_integration PRIVATE pycc_core GTest::gtest_main)
    target_include_directories(test_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
    add_test(NAME test_integration COMMAND test_integration --gtest_color=yes --gtest_print_time=1)
    set_tests_properties(test_integration PROPERTIES TIMEOUT 300)
  endif()

  if(TEST_E2E_SOURCES)
    add_executable(test_e2e ${TEST_E2E_SOURCES})
    target_link_libraries(test_e2e PRIVATE pycc_core GTest::gtest_main)
    target_include_directories(test_e2e PRIVATE ${CMAKE_SOURCE_DIR}/include)
    add_test(NAME test_e2e COMMAND test_e2e --gtest_color=yes --gtest_print_time=1)
    set_tests_properties(test_e2e PROPERTIES TIMEOUT 300)
    # Ensure the compiler binary exists before running e2e tests and set working dir
    if(TARGET pycc)
      add_dependencies(test_e2e pycc)
    endif()
    # Create an isolated run directory with timestamp to avoid polluting the build root
    string(TIMESTAMP RUN_TS "%Y%m%d-%H%M%S")
    set(RUN_DIR ${CMAKE_BINARY_DIR}/run-${RUN_TS})
    file(MAKE_DIRECTORY ${RUN_DIR})
    set_tests_properties(test_e2e PROPERTIES WORKING_DIRECTORY ${RUN_DIR})
  endif()

  # Runtime-only test target for fast, isolated runtime/GC testing
  file(GLOB TEST_RUNTIME_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/test/pycc/unit/test_runtime_*.cpp)
  if(TEST_RUNTIME_SOURCES)
    add_executable(test_runtime_only ${TEST_RUNTIME_SOURCES})
    target_link_libraries(test_runtime_only PRIVATE pycc_runtime GTest::gtest_main)
    target_include_directories(test_runtime_only PRIVATE ${CMAKE_SOURCE_DIR}/include)
    add_test(NAME test_runtime_only COMMAND test_runtime_only --gtest_color=yes --gtest_print_time=1)
    set_tests_properties(test_runtime_only PROPERTIES TIMEOUT 300)
  endif()
endif()
