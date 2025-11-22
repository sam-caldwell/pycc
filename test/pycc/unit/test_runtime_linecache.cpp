/***
 * Name: test_runtime_linecache
 * Purpose: Verify linecache.getline runtime shim.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeLinecache, GetLine) {
  gc_reset_for_tests();
  const char* fname = "_lc_tmp.txt";
  (void)os_remove(fname);
  void* content = string_from_cstr("first\nsecond\nthird\n");
  ASSERT_TRUE(io_write_file(fname, content));
  void* p = string_from_cstr(fname);
  void* l2 = linecache_getline(p, 2);
  ASSERT_NE(l2, nullptr);
  std::string s2(string_data(l2), string_len(l2));
  EXPECT_EQ(s2, std::string("second"));
  (void)os_remove(fname);
}

