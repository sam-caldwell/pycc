/***
 * Name: test_runtime_bytes_more
 * Purpose: Cover bytes_find and bytearray_extend_from_bytes.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeBytes, FindSubsequence) {
  gc_reset_for_tests();
  void* b = bytes_new("hello world", 11);
  void* n = bytes_new("world", 5);
  ASSERT_EQ(bytes_find(b, n), 6);
  void* m = bytes_new("zzz", 3);
  ASSERT_EQ(bytes_find(b, m), -1);
  ASSERT_EQ(bytes_find(b, bytes_new(nullptr, 0)), 0); // empty needle
}

TEST(RuntimeByteArray, ExtendFromBytesRespectsCapacity) {
  gc_reset_for_tests();
  void* a = bytearray_new(4); // cap >= 8
  void* src = bytes_new("ABCDEFG", 7);
  bytearray_extend_from_bytes(a, src);
  // length becomes min(cap, src len)
  EXPECT_EQ(bytearray_len(a), 7u);
  // Extending beyond capacity is capped (no reallocation)
  void* more = bytes_new("HIJ", 3);
  bytearray_extend_from_bytes(a, more);
  EXPECT_EQ(bytearray_len(a), 8u); // initial cap==8
}

