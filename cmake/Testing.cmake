include_guard(GLOBAL)

include(CTest)
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
    add_test(NAME test_unit COMMAND test_unit)
  endif()

  if(TEST_INTEGRATION_SOURCES)
    add_executable(test_integration ${TEST_INTEGRATION_SOURCES})
    target_link_libraries(test_integration PRIVATE pycc_core GTest::gtest_main)
    target_include_directories(test_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
    add_test(NAME test_integration COMMAND test_integration)
  endif()

  if(TEST_E2E_SOURCES)
    add_executable(test_e2e ${TEST_E2E_SOURCES})
    target_link_libraries(test_e2e PRIVATE pycc_core GTest::gtest_main)
    target_include_directories(test_e2e PRIVATE ${CMAKE_SOURCE_DIR}/include)
    add_test(NAME test_e2e COMMAND test_e2e)
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
endif()
