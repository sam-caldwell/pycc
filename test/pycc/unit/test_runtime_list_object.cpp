/**
 * Name: test_runtime_list_object
 * Purpose: Exercise list/object runtime mutators and basic behavior.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeList, PushAndLen) {
  gc_reset_for_tests();
  gc_set_threshold(1024);
  gc_set_background(true);
  gc_set_barrier_mode(0); // incremental-update

  void* list = nullptr;
  // push a few boxed values
  for (int i = 0; i < 10; ++i) {
    void* bi = box_int(i);
    list_push_slot(&list, bi);
  }
  EXPECT_EQ(list_len(list), 10u);
}

TEST(RuntimeObject, SetAndGet) {
  gc_reset_for_tests();
  gc_set_threshold(1024);
  gc_set_background(true);
  gc_set_barrier_mode(1); // SATB

  void* obj = object_new(3);
  void* b0 = box_int(42);
  void* b1 = box_bool(true);
  object_set(obj, 0, b0);
  object_set(obj, 1, b1);
  EXPECT_EQ(object_field_count(obj), 3u);
  EXPECT_EQ(object_get(obj, 0), b0);
  EXPECT_EQ(object_get(obj, 1), b1);
  EXPECT_EQ(object_get(obj, 2), nullptr);
}

