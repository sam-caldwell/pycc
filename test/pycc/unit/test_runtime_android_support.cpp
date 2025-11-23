/***
 * Name: test_runtime_android_support
 * Purpose: Cover _android_support runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeAndroidSupport, PlatformLibdirLdflags) {
  void* p = android_platform();
  ASSERT_NE(p, nullptr);
  EXPECT_GT(string_len(p), 0u);
  void* dir = android_default_libdir();
  ASSERT_NE(dir, nullptr);
  EXPECT_EQ(string_len(dir), 0u);
  void* lf = android_ldflags();
  ASSERT_NE(lf, nullptr);
  EXPECT_EQ(list_len(lf), 0u);
}

