/***
 * Name: test_runtime_getpass
 * Purpose: Verify getpass.getuser/getpass runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeGetpass, GetUserAndGetpass) {
  gc_reset_for_tests();
  void* u = getpass_getuser();
  ASSERT_NE(u, nullptr);
  EXPECT_GE(string_len(u), 0u);
  void* p = getpass_getpass(nullptr);
  ASSERT_NE(p, nullptr);
  // getpass returns empty string in this subset
  EXPECT_EQ(string_len(p), 0u);
}

