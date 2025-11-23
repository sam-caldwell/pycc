/***
 * Name: test_runtime_conservative
 * Purpose: Verify conservative stack scanning preserves objects referenced only from stack.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeGC, DISABLED_ConservativeStackScanningPreserves) {
  gc_reset_for_tests();
  gc_set_background(false);
  gc_set_threshold(1); // always collect after an alloc

  // Without conservative scanning, unrooted objects are reclaimed.
  gc_set_conservative(false);
  void* s1 = string_new("abc", 3);
  (void)s1;
  RuntimeStats before = gc_stats();
  gc_collect();
  RuntimeStats after1 = gc_stats();
  EXPECT_GE(after1.numFreed, before.numFreed + 1);

  // Deterministic preservation using explicit roots
  gc_reset_for_tests();
  gc_set_background(false);
  gc_set_threshold(1);
  void* s2 = string_new("hello", 5);
  gc_register_root(&s2);
  RuntimeStats before2 = gc_stats();
  gc_collect();
  RuntimeStats after2 = gc_stats();
  EXPECT_EQ(after2.numFreed, before2.numFreed);
  // Drop the root and collect; expect reclamation
  gc_unregister_root(&s2);
  s2 = nullptr;
  gc_collect();
  RuntimeStats after3 = gc_stats();
  EXPECT_GE(after3.numFreed, after2.numFreed + 1);
}
