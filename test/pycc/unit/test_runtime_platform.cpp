/***
 * Name: test_runtime_platform
 * Purpose: Verify platform.system/machine/release/version runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimePlatform, BasicStrings) {
  gc_reset_for_tests();
  void* s = platform_system();
  void* m = platform_machine();
  void* r = platform_release();
  void* v = platform_version();
  ASSERT_NE(s, nullptr); ASSERT_NE(m, nullptr); ASSERT_NE(r, nullptr); ASSERT_NE(v, nullptr);
  ASSERT_GT(string_len(s), 0u);
  ASSERT_GT(string_len(m), 0u);
  ASSERT_GT(string_len(r), 0u);
  ASSERT_GT(string_len(v), 0u);
}

