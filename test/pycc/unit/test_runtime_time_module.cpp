/***
 * Name: test_runtime_time_module
 * Purpose: Validate time module shims behavior (monotonicity, sleep, ns variants).
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"
#include <thread>

using namespace pycc::rt;

TEST(RuntimeTime, MonotonicAndPerfCounterNonDecreasing) {
  double m1 = time_monotonic();
  double p1 = time_perf_counter();
  time_sleep(0.002);
  double m2 = time_monotonic();
  double p2 = time_perf_counter();
  EXPECT_LE(m1, m2);
  EXPECT_LE(p1, p2);
}

TEST(RuntimeTime, TimeFunctionsReturnPlausibleValues) {
  double t = time_time();
  int64_t tn = time_time_ns();
  EXPECT_GT(t, 0.0);
  EXPECT_GT(tn, 0);
}

TEST(RuntimeTime, SleepDelays) {
  double m1 = time_monotonic();
  time_sleep(0.005);
  double m2 = time_monotonic();
  EXPECT_GE(m2 - m1, 0.004);
}

