/***
 * Name: test_runtime_bisect_insort
 * Purpose: Verify bisect insort_left/right mutate the list as expected.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeBisect, InsortBasic) {
  gc_reset_for_tests();
  void* lst = list_new(0);
  list_push_slot(&lst, box_int(1));
  list_push_slot(&lst, box_int(2));
  list_push_slot(&lst, box_int(4));
  // insort_right at value 2
  bisect_insort_right(&lst, box_int(2));
  EXPECT_EQ(list_len(lst), 4u);
  EXPECT_EQ(bisect_left(lst, box_int(2)), 1);
  EXPECT_EQ(bisect_right(lst, box_int(2)), 3);
  // insort_left at value 2
  bisect_insort_left(&lst, box_int(2));
  EXPECT_EQ(list_len(lst), 5u);
  EXPECT_EQ(bisect_left(lst, box_int(2)), 1);
  EXPECT_EQ(bisect_right(lst, box_int(2)), 4);
}
