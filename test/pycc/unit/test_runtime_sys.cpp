/***
 * Name: test_runtime_sys
 * Purpose: Verify sys runtime shims: platform/version/maxsize and test-safe exit.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"
#include <cstring>

using namespace pycc::rt;

TEST(RuntimeSys, PlatformVersionMaxsize) {
  gc_reset_for_tests();
  void* p = sys_platform();
  ASSERT_NE(p, nullptr);
  const char* ps = string_data(p);
  ASSERT_TRUE(std::strcmp(ps, "darwin") == 0 || std::strcmp(ps, "linux") == 0 || std::strcmp(ps, "win32") == 0 || std::strcmp(ps, "unknown") == 0);
  void* v = sys_version();
  ASSERT_NE(v, nullptr);
  ASSERT_GE(string_len(v), 3u);
  int64_t m = sys_maxsize();
  ASSERT_GT(m, 1000);
}

TEST(RuntimeSys, ExitDoesNotTerminateInTests) {
  gc_reset_for_tests();
  sys_exit(0);
  SUCCEED();
}

