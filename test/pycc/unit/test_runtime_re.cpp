/***
 * Name: test_runtime_re
 * Purpose: Cover re module runtime shims: search/match/fullmatch/findall/split/sub/subn/escape and flags/counts.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeRe, SearchMatchFullmatch) {
  gc_reset_for_tests();
  void* pat = string_from_cstr("a+");
  void* txt = string_from_cstr("baaa");
  void* ms = re_search(pat, txt, 0);
  ASSERT_NE(ms, nullptr);
  // match object: [0]=start, [1]=end, [2]=group0 string
  EXPECT_EQ(box_int_value(object_get(ms, 0)), 1);
  EXPECT_EQ(box_int_value(object_get(ms, 1)), 4);
  void* g0 = object_get(ms, 2);
  ASSERT_NE(g0, nullptr);
  EXPECT_EQ(string_len(g0), 3u);

  void* mm = re_match(pat, txt, 0);
  EXPECT_EQ(mm, nullptr);
  void* fm = re_fullmatch(pat, string_from_cstr("aaa"), 0);
  ASSERT_NE(fm, nullptr);
}

TEST(RuntimeRe, FindallSplit) {
  gc_reset_for_tests();
  void* xs = re_findall(string_from_cstr("a+"), string_from_cstr("baaa caa"), 0);
  ASSERT_EQ(list_len(xs), 2u);
  EXPECT_EQ(string_len(list_get(xs, 0)), 3u);
  EXPECT_EQ(string_len(list_get(xs, 1)), 2u);

  void* parts = re_split(string_from_cstr(","), string_from_cstr("a,b,c"), 1, 0);
  ASSERT_EQ(list_len(parts), 2u);
  EXPECT_EQ(string_len(list_get(parts, 0)), 1u);
  EXPECT_EQ(string_len(list_get(parts, 1)), 3u);
}

TEST(RuntimeRe, SubSubnEscapeFlags) {
  gc_reset_for_tests();
  void* out = re_sub(string_from_cstr("a+"), string_from_cstr("x"), string_from_cstr("baaa"), 1, 0);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(string_len(out), 2u);
  EXPECT_EQ(std::memcmp(string_data(out), "bx", 2), 0);

  void* resn = re_subn(string_from_cstr("a"), string_from_cstr("-"), string_from_cstr("aba"), 0, 0);
  ASSERT_EQ(list_len(resn), 2u);
  void* s = list_get(resn, 0);
  EXPECT_EQ(std::memcmp(string_data(s), "-b-", 3), 0);
  EXPECT_EQ(box_int_value(list_get(resn, 1)), 2);

  void* esc = re_escape(string_from_cstr("a+b"));
  ASSERT_NE(esc, nullptr);
  EXPECT_EQ(std::memcmp(string_data(esc), "a\\+b", 4), 0);

  // Flags: IGNORECASE (2)
  void* ms = re_search(string_from_cstr("A+"), string_from_cstr("baaa"), 2);
  ASSERT_NE(ms, nullptr);
}

TEST(RuntimeRe, FinditerReturnsMatchObjects) {
  gc_reset_for_tests();
  void* it = re_finditer(string_from_cstr("a+"), string_from_cstr("baaa caa"), 0);
  ASSERT_EQ(list_len(it), 2u);
  void* m0 = list_get(it, 0);
  EXPECT_EQ(box_int_value(object_get(m0, 0)), 1);
  EXPECT_EQ(box_int_value(object_get(m0, 1)), 4);
  void* m1 = list_get(it, 1);
  EXPECT_EQ(box_int_value(object_get(m1, 0)), 6);
  EXPECT_EQ(box_int_value(object_get(m1, 1)), 8);
}

TEST(RuntimeRe, MultilineAndDotAllFlagsApproximate) {
  gc_reset_for_tests();
  // MULTILINE (0x08): pattern ^a should match after newline as well
  void* ms = re_findall(string_from_cstr("^a"), string_from_cstr("b\na"), 0x08);
  ASSERT_EQ(list_len(ms), 1u);
  // DOTALL (0x20): dot should span newline
  void* all = re_findall(string_from_cstr("a.b"), string_from_cstr("a\nb"), 0x20);
  ASSERT_EQ(list_len(all), 1u);
}
