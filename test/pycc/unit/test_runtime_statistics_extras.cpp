/***
 * Name: test_runtime_statistics_extras
 * Purpose: Verify statistics.stdev/pvariance runtime shims.
 */
#include <gtest/gtest.h>
#include <cmath>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeStatisticsExtras, BasicValues) {
  gc_reset_for_tests();
  void* lst = list_new(0);
  list_push_slot(&lst, box_int(1));
  list_push_slot(&lst, box_int(2));
  list_push_slot(&lst, box_int(3));
  double sd = statistics_stdev(lst);
  double pv = statistics_pvariance(lst);
  EXPECT_NEAR(sd, 1.0, 1e-9);             // sample stdev of [1,2,3]
  EXPECT_NEAR(pv, 2.0/3.0, 1e-12);        // population variance
}

TEST(RuntimeStatisticsExtras, EdgeCases) {
  gc_reset_for_tests();
  void* empty = list_new(0);
  EXPECT_EQ(statistics_stdev(empty), 0.0);
  EXPECT_EQ(statistics_pvariance(empty), 0.0);
  void* one = list_new(0);
  list_push_slot(&one, box_int(42));
  EXPECT_EQ(statistics_stdev(one), 0.0);
  EXPECT_EQ(statistics_pvariance(one), 0.0);
}

