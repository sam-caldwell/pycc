/***
 * Name: test_runtime__ast
 * Purpose: Cover _ast runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(Runtime_Ast, DumpIterWalkCopyFixDoc) {
  void* s = ast_dump(string_from_cstr("x"));
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(string_len(s), 0u);
  void* it = ast_iter_fields(string_from_cstr("x"));
  ASSERT_NE(it, nullptr);
  EXPECT_EQ(list_len(it), 0u);
  void* w = ast_walk(string_from_cstr("x"));
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(list_len(w), 0u);
  void* newn = string_from_cstr("n");
  void* oldn = string_from_cstr("o");
  void* c = ast_copy_location(newn, oldn);
  EXPECT_EQ(c, newn);
  void* f = ast_fix_missing_locations(newn);
  EXPECT_EQ(f, newn);
  void* d = ast_get_docstring(newn);
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(string_len(d), 0u);
}

