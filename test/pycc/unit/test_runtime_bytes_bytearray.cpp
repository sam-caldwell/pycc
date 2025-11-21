/***
 * Name: test_runtime_bytes_bytearray
 * Purpose: Validate bytes and bytearray helpers in the runtime.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeBytes, NewLenDataSliceConcat) {
  gc_reset_for_tests();
  const unsigned char raw[] = {0x00, 0x41, 0x42, 0xFF};
  void* b = bytes_new(raw, sizeof(raw));
  ASSERT_EQ(bytes_len(b), 4u);
  EXPECT_EQ(bytes_data(b)[0], 0x00);
  EXPECT_EQ(bytes_data(b)[3], static_cast<unsigned char>(0xFF));
  void* s = bytes_slice(b, 1, 2);
  ASSERT_EQ(bytes_len(s), 2u);
  EXPECT_EQ(bytes_data(s)[0], 'A');
  EXPECT_EQ(bytes_data(s)[1], 'B');
  void* c = bytes_concat(b, s);
  ASSERT_EQ(bytes_len(c), 6u);
  EXPECT_EQ(bytes_data(c)[4], 'A');
}

TEST(RuntimeByteArray, NewSetGetAppend) {
  gc_reset_for_tests();
  void* a = bytearray_new(2);
  ASSERT_EQ(bytearray_len(a), 2u);
  EXPECT_EQ(bytearray_get(a, 0), 0);
  bytearray_set(a, 0, 0x7F);
  EXPECT_EQ(bytearray_get(a, 0), 0x7F);
  // Append within capacity works; after capacity, append is a no-op in this subset
  bytearray_append(a, 0x01);
  EXPECT_EQ(bytearray_len(a), 3u);
}

