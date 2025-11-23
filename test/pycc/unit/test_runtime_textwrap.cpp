/***
 * Name: test_runtime_textwrap
 * Purpose: Verify textwrap.fill/shorten runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeTextwrap, FillAndShorten) {
  gc_reset_for_tests();
  void* s = string_from_cstr("This is a test of wrap");
  void* f = textwrap_fill(s, 6);
  std::string fs(string_data(f), string_len(f));
  EXPECT_EQ(fs, std::string("This\nis a\ntest\nof\nwrap"));
  void* sh = textwrap_shorten(string_from_cstr("This is a test"), 8);
  std::string ss(string_data(sh), string_len(sh));
  EXPECT_EQ(ss, std::string("This..."));
}

