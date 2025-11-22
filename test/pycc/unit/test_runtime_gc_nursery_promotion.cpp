/***
 * Name: test_runtime_gc_nursery_promotion
 * Purpose: Exercise young/old survivor path and remembered-set barriers by mutating objects across collections.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeGC, NurserySurvivalAndPromotionCoverage) {
  gc_reset_for_tests();
  gc_set_threshold(1024); // trigger frequent collections
  gc_set_barrier_mode(1); // enable SATB pre-barrier to exercise remembered set

  void* list = nullptr;
  gc_register_root(&list);
  list = list_new(4);
  // Push a number of elements, collecting mid-way to cause some survivors
  for (int i = 0; i < 100; ++i) {
    void* val = box_int(i);
    list_push_slot(&list, val);
    if (i % 10 == 0) { gc_collect(); }
  }
  // Validate length preserved across collections
  EXPECT_EQ(list_len(list), 100u);

  // Mutate interior pointers and collect again to exercise pre-barrier/remembered set
  for (int i = 0; i < 50; ++i) {
    list_set(list, static_cast<std::size_t>(i), box_int(1000 + i));
  }
  gc_collect();

  // Spot-check a few mutated slots
  for (int i = 0; i < 5; ++i) {
    void* got = list_get(list, static_cast<std::size_t>(i));
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(box_int_value(got), 1000 + i);
  }

  // Cleanup
  gc_unregister_root(&list);
  gc_collect();
}

