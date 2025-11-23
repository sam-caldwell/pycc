/***
 * Name: test_runtime_keyword
 * Purpose: Verify keyword.iskeyword and kwlist runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeKeyword, IsKeyword) {
  gc_reset_for_tests();
  void* s1 = string_from_cstr("for");
  void* s2 = string_from_cstr("x");
  EXPECT_TRUE(keyword_iskeyword(s1));
  EXPECT_FALSE(keyword_iskeyword(s2));
}

TEST(RuntimeKeyword, KwlistContainsKnown) {
  gc_reset_for_tests();
  void* lst = keyword_kwlist();
  ASSERT_NE(lst, nullptr);
  ASSERT_GT(list_len(lst), 30u);
  // ensure "for" present
  bool found = false;
  for (std::size_t i = 0; i < list_len(lst); ++i) {
    void* s = list_get(lst, i);
    if (!s) continue;
    if (std::string(string_data(s), string_len(s)) == "for") { found = true; break; }
  }
  EXPECT_TRUE(found);
}

