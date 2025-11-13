/***
 * Name: test_runtime_boxed
 * Purpose: Validate boxed primitive allocations and accessors; exercise GC threshold.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeBoxed, IntFloatBoolRoundTrip) {
  gc_reset_for_tests();
  gc_set_threshold(64);

  void* bi = box_int(1234567890123LL);
  void* bf = box_float(3.14159);
  void* bb = box_bool(true);

  EXPECT_EQ(box_int_value(bi), 1234567890123LL);
  EXPECT_NEAR(box_float_value(bf), 3.14159, 1e-9);
  EXPECT_TRUE(box_bool_value(bb));
}

