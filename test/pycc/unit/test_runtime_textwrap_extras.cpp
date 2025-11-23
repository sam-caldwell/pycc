/***
 * Name: test_runtime_textwrap_extras
 * Purpose: Verify textwrap.wrap/dedent runtime shims.
 */
#include <gtest/gtest.h>
#include <string>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeTextwrapExtras, WrapDedentIndent) {
  gc_reset_for_tests();
  void* s = string_from_cstr("This is a test of wrap");
  void* lst = textwrap_wrap(s, 6);
  ASSERT_EQ(list_len(lst), 5u);
  std::string l0(string_data(list_get(lst,0)), string_len(list_get(lst,0)));
  std::string l1(string_data(list_get(lst,1)), string_len(list_get(lst,1)));
  std::string l2(string_data(list_get(lst,2)), string_len(list_get(lst,2)));
  std::string l3(string_data(list_get(lst,3)), string_len(list_get(lst,3)));
  std::string l4(string_data(list_get(lst,4)), string_len(list_get(lst,4)));
  EXPECT_EQ(l0, "This");
  EXPECT_EQ(l1, "is a");
  EXPECT_EQ(l2, "test");
  EXPECT_EQ(l3, "of");
  EXPECT_EQ(l4, "wrap");

  void* d = textwrap_dedent(string_from_cstr("    line1\n      line2\n    line3"));
  std::string ds(string_data(d), string_len(d));
  EXPECT_EQ(ds, std::string("line1\n  line2\nline3"));

  void* ind = textwrap_indent(string_from_cstr("A\nB"), string_from_cstr("> "));
  std::string is(string_data(ind), string_len(ind));
  EXPECT_EQ(is, std::string("> A\n> B"));
}
