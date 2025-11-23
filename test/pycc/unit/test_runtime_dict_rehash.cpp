/***
 * Name: test_runtime_dict_rehash
 * Purpose: Drive dict_set through rehash path and validate length/lookup.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeDict, RehashAndLookup) {
  gc_reset_for_tests();
  void* d = nullptr;
  // Insert > 0.7 * cap (cap starts at 8 when created), so insert 6+ entries
  for (int i = 0; i < 12; ++i) {
    char buf[8]; std::snprintf(buf, sizeof(buf), "k%02d", i);
    void* k = string_from_cstr(buf);
    void* v = box_int(i);
    dict_set(&d, k, v);
  }
  ASSERT_NE(d, nullptr);
  EXPECT_GE(dict_len(d), 12u);
  // Verify a few lookups
  void* k5 = string_from_cstr("k05");
  void* v5 = dict_get(d, k5);
  ASSERT_NE(v5, nullptr);
  EXPECT_EQ(box_int_value(v5), 5);
}

