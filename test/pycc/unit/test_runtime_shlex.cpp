/***
 * Name: test_runtime_shlex
 * Purpose: Verify shlex.split/join runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeShlex, SplitJoin) {
  gc_reset_for_tests();
  void* s = string_from_cstr("a 'b c' d\\ e \"f g\"");
  void* lst = shlex_split(s);
  ASSERT_EQ(list_len(lst), 5u);
  EXPECT_EQ(std::string(string_data(list_get(lst,0)), string_len(list_get(lst,0))), std::string("a"));
  EXPECT_EQ(std::string(string_data(list_get(lst,1)), string_len(list_get(lst,1))), std::string("b c"));
  // join produces shell-escaped string
  void* joined = shlex_join(lst);
  std::string js(string_data(joined), string_len(joined));
  EXPECT_NE(js.find("'b c'"), std::string::npos);
}

