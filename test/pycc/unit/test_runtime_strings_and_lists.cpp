/***
 * Name: test_runtime_strings_and_lists
 * Purpose: Cover string ops (concat/slice/repeat/contains/eq) and list negative indexing via C ABI wrappers.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

extern "C" void* pycc_list_get(void* list, long long index);
extern "C" void  pycc_list_set(void* list, long long index, void* value);

TEST(RuntimeStrings, ConcatSliceRepeatContainsEq) {
  gc_reset_for_tests();
  void* a = string_from_cstr("ab");
  void* b = string_from_cstr("cd");
  void* c = string_concat(a, b);
  ASSERT_EQ(string_len(c), 4u);
  EXPECT_EQ(std::memcmp(string_data(c), "abcd", 4), 0);

  void* s0 = string_slice(c, 0, 2);
  EXPECT_EQ(string_len(s0), 2u);
  EXPECT_EQ(std::memcmp(string_data(s0), "ab", 2), 0);

  void* r = string_repeat(a, 3);
  EXPECT_EQ(string_len(r), 6u);
  EXPECT_EQ(std::memcmp(string_data(r), "ababab", 6), 0);

  void* needle = string_from_cstr("bc");
  EXPECT_TRUE(string_contains(c, needle));
  void* no = string_from_cstr("xy");
  EXPECT_FALSE(string_contains(c, no));

  // equality through public header (string_eq exposed as pycc_string_eq in codegen only)
  // mimic equality by compare len+data here
  void* cc = string_from_cstr("abcd");
  EXPECT_EQ(string_len(c), string_len(cc));
  EXPECT_EQ(std::memcmp(string_data(c), string_data(cc), string_len(c)), 0);
}

TEST(RuntimeLists, NegativeIndexGetSet) {
  gc_reset_for_tests();
  // Build a list [x,y,z]
  void* xs = list_new(4);
  void* x = string_from_cstr("x");
  void* y = string_from_cstr("y");
  void* z = string_from_cstr("z");
  list_push_slot(&xs, x);
  list_push_slot(&xs, y);
  list_push_slot(&xs, z);
  ASSERT_EQ(list_len(xs), 3u);

  // Get with negative index -1 => z
  void* last = pycc_list_get(xs, -1);
  ASSERT_NE(last, nullptr);
  EXPECT_EQ(std::memcmp(string_data(last), "z", 1), 0);

  // Set with negative index -3 => first element becomes 'w'
  void* w = string_from_cstr("w");
  pycc_list_set(xs, -3, w);
  void* first = list_get(xs, 0);
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(std::memcmp(string_data(first), "w", 1), 0);
}

