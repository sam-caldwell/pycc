/***
 * Name: test_runtime_subprocess
 * Purpose: Cover subprocess shims: run/call/check_call behavior and exceptions.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeSubprocess, RunAndCall) {
  gc_reset_for_tests();
  void* t = string_from_cstr("true");
  int32_t rc1 = subprocess_run(t);
  int32_t rc2 = subprocess_call(t);
  EXPECT_EQ(rc1, 0);
  EXPECT_EQ(rc2, 0);
}

TEST(RuntimeSubprocess, CheckCallRaisesOnFailure) {
  gc_reset_for_tests();
  // Use a shell exit code to portably produce non-zero
  void* cmd = string_from_cstr("sh -c 'exit 3'");
  int32_t rc = subprocess_check_call(cmd);
  EXPECT_NE(rc, 0);
  ASSERT_TRUE(rt_has_exception());
  void* exc = rt_current_exception();
  ASSERT_STREQ(string_data(rt_exception_type(exc)), "CalledProcessError");
  rt_clear_exception();
}

