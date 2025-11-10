cmake_minimum_required(VERSION 3.25)

file(GLOB INTEGRATION_TEST_SOURCES CONFIGURE_DEPENDS *.cpp)

if(INTEGRATION_TEST_SOURCES)
  add_executable(pycc_tests_integration ${INTEGRATION_TEST_SOURCES})
  target_link_libraries(pycc_tests_integration PRIVATE pycc_driver GTest::gtest_main)
  target_include_directories(pycc_tests_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_features(pycc_tests_integration PRIVATE cxx_std_23)
  gtest_discover_tests(pycc_tests_integration DISCOVERY_MODE PRE_TEST)
endif()
