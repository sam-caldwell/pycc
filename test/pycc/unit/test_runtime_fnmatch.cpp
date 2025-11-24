/*
 * Name: test_runtime_fnmatch
 * Purpose: Validate runtime fnmatch.* shims for full pattern support and behavior.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

static void* S(const char* s) { return string_from_cstr(s); }

TEST(RuntimeFnmatch, BasicStarQuestion) {
  gc_reset_for_tests();
  EXPECT_TRUE(fnmatch_fnmatch(S("file.txt"), S("file*.txt")));
  EXPECT_FALSE(fnmatch_fnmatch(S("file.txt"), S("*.log")));
  EXPECT_TRUE(fnmatch_fnmatchcase(S("abc"), S("a?c")));
  EXPECT_FALSE(fnmatch_fnmatchcase(S("Abc"), S("a?c")));
}

TEST(RuntimeFnmatch, CharClassAndNegationRange) {
  gc_reset_for_tests();
  // Class includes
  EXPECT_TRUE(fnmatch_fnmatchcase(S("f.txt"), S("f.[tx]xt")));
  EXPECT_FALSE(fnmatch_fnmatchcase(S("f.rxt"), S("f.[tx]xt")));
  // Negation (leading !)
  EXPECT_TRUE(fnmatch_fnmatchcase(S("f.txt"), S("f.[!r]xt")));
  EXPECT_FALSE(fnmatch_fnmatchcase(S("f.rxt"), S("f.[!r]xt")));
  // Range
  EXPECT_TRUE(fnmatch_fnmatchcase(S("f.bxt"), S("f.[a-c]xt")));
  EXPECT_FALSE(fnmatch_fnmatchcase(S("f.dxt"), S("f.[a-c]xt")));
  // Literal ']' as first class character
  EXPECT_TRUE(fnmatch_fnmatchcase(S("x]y"), S("x[]]y")));
}

TEST(RuntimeFnmatch, FilterAndTranslateInterop) {
  gc_reset_for_tests();
  // filter
  void* names = list_new(0);
  list_push_slot(&names, S("a.py"));
  list_push_slot(&names, S("b.txt"));
  list_push_slot(&names, S("c.py"));
  void* out = fnmatch_filter(names, S("*.py"));
  ASSERT_EQ(list_len(out), 2u);
  EXPECT_EQ(std::string(string_data(list_get(out, 0)), string_len(list_get(out, 0))), std::string("a.py"));
  EXPECT_EQ(std::string(string_data(list_get(out, 1)), string_len(list_get(out, 1))), std::string("c.py"));

  // translate + re should agree with fnmatch on a few cases
  auto check = [&](const char* name, const char* pat){
    void* rx = fnmatch_translate(S(pat));
    void* m = re_match(rx, S(name), 0);
    bool re_ok = (m != nullptr);
    bool fn_ok = fnmatch_fnmatchcase(S(name), S(pat));
    EXPECT_EQ(re_ok, fn_ok) << "name=" << name << " pat=" << pat;
  };
  check("abc", "a?c");
  check("xyz", "[x-z][x-z][x-z]");
  check("AX", "[!a-z][!0-9]");
}

