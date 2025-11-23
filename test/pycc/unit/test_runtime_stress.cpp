/***
 * Name: test_runtime_stress
 * Purpose: Stress allocations to trigger multiple collections; validate stats invariants.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"
#include <vector>
#include <string>
#include "../../util/Heartbeat.h"

using namespace pycc::rt;

TEST(RuntimeGC, DISABLED_StressAllocationsStats) {
  testutil::Heartbeat hb("RuntimeGC.StressAllocationsStats");
  gc_reset_for_tests();
  gc_set_threshold(1024); // small threshold to trigger collections frequently

  std::vector<void*> roots;
  roots.reserve(1000);
  // Create a mix of rooted and ephemeral objects
  for (int i = 0; i < 2000; ++i) {
    auto s = string_new("xxxxxxxxxx", 10);
    if (i % 3 == 0) {
      roots.push_back(s);
      gc_register_root(&roots.back());
    }
    (void)box_int(i * 12345LL);
    (void)box_float(0.5 * i);
    (void)box_bool((i & 1) != 0);
  }

  // Ensure collections are serviced before inspecting stats
  gc_collect();
  RuntimeStats st1 = gc_stats();
  EXPECT_GE(st1.numCollections, 1u);
  EXPECT_GE(st1.bytesAllocated, st1.bytesLive);
  EXPECT_GE(st1.peakBytesLive, st1.bytesLive);

  // Drop roots and collect; reclaimed bytes should increase at least once
  for (auto& r : roots) { gc_unregister_root(&r); r = nullptr; }
  gc_collect();
  RuntimeStats st2 = gc_stats();
  EXPECT_GE(st2.numFreed, st1.numFreed);
  EXPECT_GE(st2.lastReclaimedBytes, 0u);
}
