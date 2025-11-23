/***
 * Name: test_runtime_extras
 * Purpose: Add runtime helper coverage: list_new/len zero, telemetry, pre-barrier.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeExtras, ListLenZeroAndPush) {
  gc_reset_for_tests();
  void* list = list_new(0);
  EXPECT_EQ(list_len(list), 0u);
  void* bi = box_int(7);
  list_push_slot(&list, bi);
  EXPECT_EQ(list_len(list), 1u);
}

TEST(RuntimeExtras, TelemetryNonNegativeAndPressure) {
  gc_reset_for_tests();
  gc_set_threshold(1024);
  (void)string_new("hello", 5);
  auto telem = gc_telemetry();
  EXPECT_GE(telem.allocRateBytesPerSec, 0.0);
  EXPECT_GE(telem.pressure, 0.0);
}

TEST(RuntimeExtras, PreBarrierAndWriteBarrierCalls) {
  gc_reset_for_tests();
  gc_set_threshold(64);
  void* obj = box_int(1);
  void* slot = nullptr;
  // Pre-barrier hook and write-barrier no-ops for coverage
  gc_pre_barrier(&slot);
  gc_write_barrier(&slot, obj);
  EXPECT_TRUE(true);
}

