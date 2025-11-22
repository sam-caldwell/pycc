/***
 * Name: test_runtime_aix_support
 * Purpose: Cover _aix_support runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeAIXSupport, PlatformLibpathLdflags) {
  void* p = aix_platform();
  ASSERT_NE(p, nullptr);
  EXPECT_GT(string_len(p), 0u);
  void* lib = aix_default_libpath();
  ASSERT_NE(lib, nullptr);
  EXPECT_EQ(string_len(lib), 0u);
  void* lf = aix_ldflags();
  ASSERT_NE(lf, nullptr);
  EXPECT_EQ(list_len(lf), 0u);
}

