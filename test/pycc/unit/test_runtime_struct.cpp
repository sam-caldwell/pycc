/***
 * Name: test_runtime_struct
 * Purpose: Verify struct pack/unpack runtime shims.
 */
#include <gtest/gtest.h>
#include <string>
#include <cmath>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeStruct, PackUnpackInt) {
  gc_reset_for_tests();
  void* fmt = string_from_cstr("<i");
  void* vals = list_new(1);
  list_push_slot(&vals, box_int(123456));
  void* b = struct_pack(fmt, vals);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(bytes_len(b), 4u);
  void* l = struct_unpack(fmt, b);
  ASSERT_EQ(list_len(l), 1u);
  EXPECT_EQ(box_int_value(list_get(l,0)), 123456);
}

TEST(RuntimeStruct, PackUnpackFloatAndByte) {
  gc_reset_for_tests();
  void* fmt = string_from_cstr("<fbB");
  void* vals = list_new(0);
  list_push_slot(&vals, box_float(1.5));
  list_push_slot(&vals, box_int(-1));
  list_push_slot(&vals, box_int(255));
  void* b = struct_pack(fmt, vals);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(bytes_len(b), 6u);
  void* l = struct_unpack(fmt, b);
  ASSERT_EQ(list_len(l), 3u);
  EXPECT_NEAR(box_float_value(list_get(l,0)), 1.5, 1e-6);
  EXPECT_EQ(box_int_value(list_get(l,1)), -1);
  EXPECT_EQ(box_int_value(list_get(l,2)), 255);
}

TEST(RuntimeStruct, Calcsize) {
  gc_reset_for_tests();
  EXPECT_EQ(struct_calcsize(string_from_cstr("<i")), 4);
  EXPECT_EQ(struct_calcsize(string_from_cstr("2B")), 2);
  EXPECT_EQ(struct_calcsize(string_from_cstr("3i")), 12);
}

