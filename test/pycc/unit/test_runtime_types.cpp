/***
 * Name: test_runtime_types
 * Purpose: Verify types.SimpleNamespace runtime shim.
 */
#include <gtest/gtest.h>
#include <string>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeTypes, SimpleNamespaceInitAndAttrs) {
  gc_reset_for_tests();
  void* pairs = list_new(0);
  void* p1 = list_new(2);
  list_push_slot(&p1, string_from_cstr("a"));
  list_push_slot(&p1, box_int(1));
  void* p2 = list_new(2);
  list_push_slot(&p2, string_from_cstr("b"));
  list_push_slot(&p2, string_from_cstr("x"));
  list_push_slot(&pairs, p1);
  list_push_slot(&pairs, p2);
  void* ns = types_simple_namespace(pairs);
  void* aval = object_get_attr(ns, string_from_cstr("a"));
  void* bval = object_get_attr(ns, string_from_cstr("b"));
  ASSERT_NE(aval, nullptr);
  ASSERT_NE(bval, nullptr);
  EXPECT_EQ(box_int_value(aval), 1);
  std::string bs(string_data(bval), string_len(bval));
  EXPECT_EQ(bs, std::string("x"));
}

