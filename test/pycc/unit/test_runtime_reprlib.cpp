/***
 * Name: test_runtime_reprlib
 * Purpose: Verify reprlib.repr runtime shim.
 */
#include <gtest/gtest.h>
#include <string>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeReprlib, ShortAndTruncated) {
  gc_reset_for_tests();
  void* lst = list_new(0);
  for (int i=0;i<20;++i) list_push_slot(&lst, box_int(i));
  void* r = reprlib_repr(lst);
  std::string s(string_data(r), string_len(r));
  ASSERT_GE(s.size(), 3u);
  // Should be truncated with ellipsis
  ASSERT_NE(s.find("..."), std::string::npos);

  void* sshort = reprlib_repr(string_from_cstr("abc"));
  std::string ss(string_data(sshort), string_len(sshort));
  EXPECT_EQ(ss, std::string("'abc'"));
}

