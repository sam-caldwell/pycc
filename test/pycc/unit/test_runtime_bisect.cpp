/***
 * Name: test_runtime_bisect
 * Purpose: Verify bisect_left/right runtime shims on sorted lists.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeBisect, Basic) {
  gc_reset_for_tests();
  void* lst = list_new(0);
  list_push_slot(&lst, box_int(1));
  list_push_slot(&lst, box_int(2));
  list_push_slot(&lst, box_int(4));
  int32_t i1 = bisect_left(lst, box_int(2));
  int32_t i2 = bisect_right(lst, box_int(2));
  int32_t i3 = bisect_left(lst, box_int(3));
  EXPECT_EQ(i1, 1);
  EXPECT_EQ(i2, 2);
  EXPECT_EQ(i3, 2);
}

