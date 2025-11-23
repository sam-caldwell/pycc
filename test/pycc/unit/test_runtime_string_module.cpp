/***
 * Name: test_runtime_string_module
 * Purpose: Verify string.capwords runtime shim.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeString, CapwordsBasic) {
  gc_reset_for_tests();
  void* s = string_from_cstr("hello   world\tfrom PYCC");
  void* r = string_capwords(s, nullptr);
  ASSERT_NE(r, nullptr);
  std::string out(string_data(r), string_len(r));
  EXPECT_EQ(out, std::string("Hello World From Pycc"));
}

TEST(RuntimeString, CapwordsWithSep) {
  gc_reset_for_tests();
  void* s = string_from_cstr("a-b-c");
  void* sep = string_from_cstr("-");
  void* r = string_capwords(s, sep);
  std::string out(string_data(r), string_len(r));
  EXPECT_EQ(out, std::string("A-B-C"));
}

