/***
 * Name: test_runtime_attributes
 * Purpose: Validate per-instance attribute dict for objects and GC retention.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeAttributes, SetGetAndRetention) {
  gc_reset_for_tests();
  gc_set_background(false); // deterministic collection
  gc_set_threshold(1); // collect on next alloc

  void* obj = object_new(1);
  // Root the object so only attribute value relies on attribute dict reachability
  gc_register_root(&obj);

  void* key = string_from_cstr("name");
  void* value = string_from_cstr("pycc");
  object_set_attr(obj, key, value);
  // Drop strong reference to value and collect; expect reachability via attribute dict
  value = nullptr;
  gc_collect();
  void* got = object_get_attr(obj, key);
  ASSERT_NE(got, nullptr);
  EXPECT_EQ(string_len(got), 4u);
  EXPECT_STREQ(string_data(got), "pycc");

  // The internal dict should exist now
  ASSERT_NE(object_get_attr_dict(obj), nullptr);

  gc_unregister_root(&obj);
}

