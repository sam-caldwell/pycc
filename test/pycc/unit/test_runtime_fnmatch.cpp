/***
 * Name: test_runtime_fnmatch
 * Purpose: Verify fnmatch.fnmatch/case/filter/translate runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeFnmatch, MatchAndTranslate) {
  gc_reset_for_tests();
  void* name = string_from_cstr("abc");
  void* pat1 = string_from_cstr("a?c");
  void* pat2 = string_from_cstr("a*d");
  EXPECT_TRUE(fnmatch_fnmatchcase(name, pat1));
  EXPECT_FALSE(fnmatch_fnmatchcase(name, pat2));
  void* rx = fnmatch_translate(pat1);
  ASSERT_NE(rx, nullptr);
  const char* rxs = string_data(rx);
  ASSERT_NE(rxs, nullptr);
  ASSERT_GT(string_len(rx), 0u);
}

TEST(RuntimeFnmatch, Filter) {
  gc_reset_for_tests();
  void* lst = list_new(0);
  list_push_slot(&lst, string_from_cstr("a"));
  list_push_slot(&lst, string_from_cstr("ab"));
  list_push_slot(&lst, string_from_cstr("b"));
  void* res = fnmatch_filter(lst, string_from_cstr("a*"));
  ASSERT_NE(res, nullptr);
  ASSERT_EQ(list_len(res), 2u);
}

