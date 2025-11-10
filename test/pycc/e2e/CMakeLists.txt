cmake_minimum_required(VERSION 3.25)

file(GLOB E2E_TEST_SOURCES CONFIGURE_DEPENDS *.cpp)

if(E2E_TEST_SOURCES)
  add_executable(pycc_tests_e2e ${E2E_TEST_SOURCES})
  target_link_libraries(pycc_tests_e2e PRIVATE pycc_driver GTest::gtest_main)
  target_include_directories(pycc_tests_e2e PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_features(pycc_tests_e2e PRIVATE cxx_std_23)
  gtest_discover_tests(pycc_tests_e2e DISCOVERY_MODE PRE_TEST)
endif()

# Add a CTest that compiles and runs a tiny program returning 7
add_test(
  NAME pycc_e2e_return_int
  COMMAND ${CMAKE_COMMAND}
    -Dpycc_exe=$<TARGET_FILE:pycc>
    -P ${CMAKE_CURRENT_SOURCE_DIR}/return_int_test.cmake)
set_tests_properties(pycc_e2e_return_int PROPERTIES
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# E2E metrics JSON
add_test(
  NAME pycc_e2e_metrics_json
  COMMAND ${CMAKE_COMMAND}
    -Dpycc_exe=$<TARGET_FILE:pycc>
    -P ${CMAKE_CURRENT_SOURCE_DIR}/metrics_json_test.cmake)
set_tests_properties(pycc_e2e_metrics_json PROPERTIES
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
