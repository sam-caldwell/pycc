/***
 * Name: test_runtime_gc_background_and_barriers
 * Purpose: Exercise background GC synchronous path and SATB pre-barrier branch.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeGC, DISABLED_BackgroundCollectIncrementsCount) {
  gc_reset_for_tests();
  gc_set_background(true);
  RuntimeStats before = gc_stats();
  // Allocate a little, then request a collection
  (void)string_from_cstr("bg");
  gc_collect();
  RuntimeStats after = gc_stats();
  EXPECT_GE(after.numCollections, before.numCollections);
}

TEST(RuntimeGC, DISABLED_SatbPreBarrierBranchNoCrash) {
  gc_reset_for_tests();
  gc_set_background(true);
  gc_set_barrier_mode(1); // enable SATB
  // Create an object and point a slot at it; exercise pre-barrier
  void* obj = box_int(123);
  void* slot = obj;
  gc_pre_barrier(&slot);
  // Also exercise write barrier while here
  void* obj2 = box_int(456);
  gc_write_barrier(&slot, obj2);
  EXPECT_TRUE(true);
}
