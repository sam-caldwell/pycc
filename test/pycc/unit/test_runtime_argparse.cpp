/***
 * Name: test_runtime_argparse
 * Purpose: Verify argparse subset runtime shims.
 */
#include <gtest/gtest.h>
#include <string>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeArgparse, StoreTrueAndInt) {
  gc_reset_for_tests();
  void* p = argparse_argument_parser();
  argparse_add_argument(p, string_from_cstr("--verbose"), string_from_cstr("store_true"));
  argparse_add_argument(p, string_from_cstr("--count"), string_from_cstr("store_int"));
  void* args = list_new(0);
  list_push_slot(&args, string_from_cstr("--verbose"));
  list_push_slot(&args, string_from_cstr("--count"));
  list_push_slot(&args, string_from_cstr("3"));
  void* d = argparse_parse_args(p, args);
  ASSERT_NE(d, nullptr);
  void* vkey = string_from_cstr("verbose");
  void* ckey = string_from_cstr("count");
  void* v = dict_get(d, vkey);
  void* c = dict_get(d, ckey);
  ASSERT_NE(v, nullptr);
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(box_bool_value(v));
  EXPECT_EQ(box_int_value(c), 3);
}

TEST(RuntimeArgparse, StoreStringEq) {
  gc_reset_for_tests();
  void* p = argparse_argument_parser();
  argparse_add_argument(p, string_from_cstr("--name"), string_from_cstr("store"));
  void* args = list_new(0);
  list_push_slot(&args, string_from_cstr("--name=alice"));
  void* d = argparse_parse_args(p, args);
  void* key = string_from_cstr("name");
  void* val = dict_get(d, key);
  ASSERT_NE(val, nullptr);
  std::string s(string_data(val), string_len(val));
  EXPECT_EQ(s, std::string("alice"));
}

