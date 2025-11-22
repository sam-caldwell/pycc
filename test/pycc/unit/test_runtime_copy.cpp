/***
 * Name: test_runtime_copy
 * Purpose: Verify shallow and deep copy behavior for lists and dicts.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeCopy, ShallowListCopy) {
  gc_reset_for_tests();
  void* lst = list_new(0);
  list_push_slot(&lst, box_int(1));
  void* c = copy_copy(lst);
  ASSERT_NE(c, nullptr);
  // Modify copy; original should remain
  list_set(c, 0, box_int(9));
  int v0 = static_cast<int>(box_int_value(list_get(lst, 0)));
  EXPECT_EQ(v0, 1);
}

TEST(RuntimeCopy, DeepDictCopy) {
  gc_reset_for_tests();
  void* inner = list_new(0); list_push_slot(&inner, box_int(1));
  void* d = dict_new(4);
  dict_set(&d, string_from_cstr("x"), inner);
  void* c = copy_deepcopy(d);
  ASSERT_NE(c, nullptr);
  // mutate copy's inner list; original should not change
  void* cx = dict_get(c, string_from_cstr("x"));
  list_set(cx, 0, box_int(7));
  void* ox = dict_get(d, string_from_cstr("x"));
  int v0 = static_cast<int>(box_int_value(list_get(ox, 0)));
  EXPECT_EQ(v0, 1);
}

