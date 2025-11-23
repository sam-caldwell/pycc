/***
 * Name: test_runtime_bytes_bytearray_edges
 * Purpose: Exercise bytes/bytearray edge behaviors (nulls, OOB, capacity limit).
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeBytesEdges, NullLenAndDataSafe) {
  EXPECT_EQ(bytes_len(nullptr), 0u);
  EXPECT_EQ(bytes_data(nullptr), nullptr);
}

TEST(RuntimeByteArrayEdges, OOBGetSetAndCapacityNoGrow) {
  gc_reset_for_tests();
  void* a = bytearray_new(0); // cap at least 8
  EXPECT_EQ(bytearray_len(a), 0u);
  // OOB get returns -1; OOB set has no effect
  EXPECT_EQ(bytearray_get(a, 0), -1);
  bytearray_set(a, 0, 0x12); // no crash
  // Fill to capacity
  for (int i = 0; i < 8; ++i) bytearray_append(a, i);
  EXPECT_EQ(bytearray_len(a), 8u);
  const int last = bytearray_get(a, 7);
  // Further appends are no-op in this subset; length should not increase and last element unchanged
  bytearray_append(a, 0xFF);
  EXPECT_EQ(bytearray_len(a), 8u);
  EXPECT_EQ(bytearray_get(a, 7), last);
}

