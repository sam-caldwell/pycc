/***
 * Name: test_runtime_apple_support
 * Purpose: Cover _apple_support runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeAppleSupport, PlatformSdkrootLdflags) {
  void* p = apple_platform();
  ASSERT_NE(p, nullptr);
  EXPECT_GT(string_len(p), 0u);
  void* sdk = apple_default_sdkroot();
  ASSERT_NE(sdk, nullptr);
  EXPECT_EQ(string_len(sdk), 0u);
  void* lf = apple_ldflags();
  ASSERT_NE(lf, nullptr);
  EXPECT_EQ(list_len(lf), 0u);
}

