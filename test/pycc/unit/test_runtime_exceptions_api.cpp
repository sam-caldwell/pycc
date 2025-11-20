/***
 * Name: test_runtime_exceptions_api
 * Purpose: Validate simple exception set/get/clear and GC protection.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeExceptions, RaiseAndClear) {
  gc_reset_for_tests();
  gc_set_background(false);
  gc_set_threshold(1);

  EXPECT_FALSE(rt_has_exception());
  rt_raise("ValueError", "bad input");
  ASSERT_TRUE(rt_has_exception());
  void* exc = rt_current_exception();
  ASSERT_NE(exc, nullptr);
  EXPECT_STREQ(string_data(rt_exception_type(exc)), "ValueError");
  EXPECT_STREQ(string_data(rt_exception_message(exc)), "bad input");

  // Force a collection; exception must remain reachable
  gc_collect();
  ASSERT_TRUE(rt_has_exception());
  ASSERT_NE(rt_current_exception(), nullptr);

  rt_clear_exception();
  EXPECT_FALSE(rt_has_exception());
  EXPECT_EQ(rt_current_exception(), nullptr);
}

