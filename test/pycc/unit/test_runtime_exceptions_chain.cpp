/***
 * Name: test_runtime_exceptions_chain
 * Purpose: Validate expanded exception helpers: cause and context fields.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeExceptions, CauseAndContext) {
  gc_reset_for_tests();
  // Create two exceptions via raises and capture them
  try { rt_raise("TypeError", "bad type"); } catch (...) {}
  void* e1 = rt_current_exception();
  ASSERT_NE(e1, nullptr);
  rt_clear_exception();
  try { rt_raise("ValueError", "bad value"); } catch (...) {}
  void* e2 = rt_current_exception();
  ASSERT_NE(e2, nullptr);
  // Link cause/context on e2
  rt_exception_set_cause(e2, e1);
  rt_exception_set_context(e2, e1);
  ASSERT_EQ(rt_exception_cause(e2), e1);
  ASSERT_EQ(rt_exception_context(e2), e1);
}

