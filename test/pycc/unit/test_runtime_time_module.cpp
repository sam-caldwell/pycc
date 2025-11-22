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

TEST(RuntimeTime, NsVariantsNonDecreasing) {
  int64_t m1 = time_monotonic_ns();
  int64_t p1 = time_perf_counter_ns();
  time_sleep(0.001);
  int64_t m2 = time_monotonic_ns();
  int64_t p2 = time_perf_counter_ns();
  EXPECT_LE(m1, m2);
  EXPECT_LE(p1, p2);
}

TEST(RuntimeTime, ProcessTimeReturnsNonNegativeAndSmallDelta) {
  double t1 = time_process_time();
  // Perform some work
  volatile double acc = 0.0; // NOLINT(cert-flp30-c)
  for (int i = 0; i < 100000; ++i) acc += 0.000001;
  double t2 = time_process_time();
  EXPECT_GE(t1, 0.0);
  EXPECT_LE(t2 - t1, 1.0);
}
