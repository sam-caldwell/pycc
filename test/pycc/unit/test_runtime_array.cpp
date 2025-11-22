/***
 * Name: test_runtime_array
 * Purpose: Verify array subset runtime shims.
 */
#include <gtest/gtest.h>
#include <string>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeArray, IntArrayOps) {
  gc_reset_for_tests();
  void* a = array_array(string_from_cstr("i"), nullptr);
  array_append(a, box_int(1));
  array_append(a, box_int(2));
  array_append(a, box_int(3));
  void* lst = array_tolist(a);
  ASSERT_EQ(list_len(lst), 3u);
  EXPECT_EQ(box_int_value(list_get(lst,0)), 1);
  EXPECT_EQ(box_int_value(list_get(lst,1)), 2);
  EXPECT_EQ(box_int_value(list_get(lst,2)), 3);
  void* v = array_pop(a);
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(box_int_value(v), 3);
  lst = array_tolist(a);
  ASSERT_EQ(list_len(lst), 2u);
}

TEST(RuntimeArray, FloatArrayOps) {
  gc_reset_for_tests();
  void* init = list_new(0);
  list_push_slot(&init, box_float(1.5));
  list_push_slot(&init, box_int(2));
  void* a = array_array(string_from_cstr("f"), init);
  void* lst = array_tolist(a);
  ASSERT_EQ(list_len(lst), 2u);
  EXPECT_NEAR(box_float_value(list_get(lst,0)), 1.5, 1e-9);
  EXPECT_NEAR(box_float_value(list_get(lst,1)), 2.0, 1e-9);
}

