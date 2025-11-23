/***
 * Name: test_runtime_statistics
 * Purpose: Verify statistics.mean/median runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeStatistics, MeanMedian) {
  gc_reset_for_tests();
  void* lst = list_new(0);
  list_push_slot(&lst, box_int(1));
  list_push_slot(&lst, box_int(2));
  list_push_slot(&lst, box_int(3));
  double m = statistics_mean(lst);
  EXPECT_NEAR(m, 2.0, 1e-12);
  double md = statistics_median(lst);
  EXPECT_NEAR(md, 2.0, 1e-12);
  // even count
  list_push_slot(&lst, box_int(4));
  double md2 = statistics_median(lst);
  EXPECT_NEAR(md2, 2.5, 1e-12);
}

