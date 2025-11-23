/***
 * Name: test_runtime_operator
 * Purpose: Verify operator.* runtime shims for numeric and boolean ops.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeOperator, NumericOps) {
  gc_reset_for_tests();
  void* a = box_int(2);
  void* b = box_int(3);
  void* s = operator_add(a,b);
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(box_int_value(s), 5);
  void* d = operator_truediv(box_int(1), box_int(2));
  ASSERT_NE(d, nullptr);
  EXPECT_NEAR(box_float_value(d), 0.5, 1e-12);
  void* n = operator_neg(box_int(5));
  EXPECT_EQ(box_int_value(n), -5);
}

TEST(RuntimeOperator, ComparisonsAndTruth) {
  gc_reset_for_tests();
  void* a = box_int(2);
  void* b = box_int(3);
  EXPECT_TRUE(operator_lt(a, b));
  EXPECT_TRUE(operator_eq(a, box_int(2)));
  EXPECT_TRUE(operator_truth(a));
  EXPECT_TRUE(operator_not_(box_int(0)));
}

