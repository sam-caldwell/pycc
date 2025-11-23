/***
 * Name: test_runtime_subprocess
 * Purpose: Cover subprocess shims: run/call/check_call behavior and exceptions.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

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
  EXPECT_EQ(rc, 3);
  ASSERT_TRUE(rt_has_exception());
  void* exc = rt_current_exception();
  ASSERT_STREQ(string_data(rt_exception_type(exc)), "CalledProcessError");
  rt_clear_exception();
}

TEST(RuntimeSubprocess, CheckCallSuccessNoException) {
  gc_reset_for_tests();
  void* t = string_from_cstr("true");
  int32_t rc = subprocess_check_call(t);
  EXPECT_EQ(rc, 0);
  EXPECT_FALSE(rt_has_exception());
}

TEST(RuntimeSubprocess, RunCallNullPtrReturnMinusOne) {
  gc_reset_for_tests();
  int32_t rc1 = subprocess_run(nullptr);
  int32_t rc2 = subprocess_call(nullptr);
  EXPECT_EQ(rc1, -1);
  EXPECT_EQ(rc2, -1);
}

TEST(RuntimeSubprocess, RunFailureNoException) {
  gc_reset_for_tests();
  // Force a non-zero exit without throwing: run() does not raise
  void* cmd = string_from_cstr("sh -c 'exit 7'");
  int32_t rc = subprocess_run(cmd);
  EXPECT_EQ(rc, 7);
  EXPECT_FALSE(rt_has_exception());
}

TEST(RuntimeSubprocess, CallFailureNoException) {
  gc_reset_for_tests();
  void* cmd = string_from_cstr("sh -c 'exit 5'");
  int32_t rc = subprocess_call(cmd);
  EXPECT_EQ(rc, 5);
  EXPECT_FALSE(rt_has_exception());
}
