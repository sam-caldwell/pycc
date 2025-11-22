/***
 * Name: test_runtime_json
 * Purpose: Validate json.dumps/loads round-trips for basic types and structures.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeJSON, DumpsPrimitives) {
  gc_reset_for_tests();
  void* n = nullptr;
  void* s1 = json_dumps(n);
  ASSERT_STREQ(string_data(s1), "null");
  void* t = box_bool(true);
  void* s2 = json_dumps(t);
  ASSERT_STREQ(string_data(s2), "true");
  void* i = box_int(123);
  void* s3 = json_dumps(i);
  ASSERT_STREQ(string_data(s3), "123");
  void* f = box_float(3.5);
  void* s4 = json_dumps(f);
  ASSERT_NE(string_data(s4), nullptr);
}

TEST(RuntimeJSON, DumpsListAndDict) {
  gc_reset_for_tests();
  void* lst = list_new(2);
  list_push_slot(&lst, box_int(1));
  list_push_slot(&lst, string_from_cstr("x"));
  void* s = json_dumps(lst);
  ASSERT_NE(s, nullptr);
  ASSERT_NE(strstr(string_data(s), "["), nullptr);
  void* d = dict_new(4);
  void* k = string_from_cstr("a");
  dict_set(&d, k, box_int(1));
  void* s2 = json_dumps(d);
  ASSERT_NE(s2, nullptr);
  ASSERT_NE(strstr(string_data(s2), "{"), nullptr);
}

TEST(RuntimeJSON, LoadsBasicShapes) {
  gc_reset_for_tests();
  auto L = [&](const char* txt){ return json_loads(string_from_cstr(txt)); };
  void* v1 = L("null"); ASSERT_EQ(v1, nullptr);
  void* v2 = L("true"); ASSERT_TRUE(box_bool_value(v2));
  void* v3 = L("123"); ASSERT_EQ(box_int_value(v3), 123);
  void* v4 = L("3.14"); ASSERT_NE(v4, nullptr);
  void* v5 = L("\"abc\""); ASSERT_STREQ(string_data(v5), "abc");
  void* arr = L("[1,2]"); ASSERT_EQ(list_len(arr), 2u);
  void* obj = L("{\"a\":1}");
  // Re-serialize and check substring for key-value pair
  void* s = json_dumps(obj);
  ASSERT_NE(s, nullptr);
  ASSERT_NE(strstr(string_data(s), "\"a\":1"), nullptr);
}

TEST(RuntimeJSON, LoadsUnicodeEscapesStrict) {
  gc_reset_for_tests();
  // \u00E9 -> é
  void* e = json_loads(string_from_cstr("\"\\u00E9\""));
  ASSERT_STREQ(string_data(e), "é");
  // Surrogate pair U+1F4A9 (pile of poo): \uD83D\uDCA9
  void* poo = json_loads(string_from_cstr("\"\\uD83D\\uDCA9\""));
  ASSERT_NE(poo, nullptr);
  // Invalid escapes raise error -> returns nullptr and exception flag set
  void* bad = json_loads(string_from_cstr("\"\\uZZZZ\""));
  ASSERT_EQ(bad, nullptr);
  ASSERT_TRUE(rt_has_exception());
  rt_clear_exception();
}

TEST(RuntimeJSON, DumpsPrettyPrint) {
  gc_reset_for_tests();
  void* lst = list_new(2);
  list_push_slot(&lst, box_int(1));
  list_push_slot(&lst, box_int(2));
  void* s = json_dumps_ex(lst, 2);
  const char* d = string_data(s);
  // Should contain newlines and indentation
  ASSERT_NE(strstr(d, "\n  1"), nullptr);
  ASSERT_NE(strstr(d, "\n]"), nullptr);
}

TEST(RuntimeJSON, DumpsEnsureAsciiAndSeparatorsAndSortKeys) {
  gc_reset_for_tests();
  void* d = dict_new(4);
  dict_set(&d, string_from_cstr("b"), string_from_cstr("é"));
  dict_set(&d, string_from_cstr("a"), box_int(1));
  // ensure_ascii=1, indent=0, item_sep=", ", kv_sep=": ", sort_keys=1
  void* out = json_dumps_opts(d, 1, 0, ", ", ": ", 1);
  const char* s = string_data(out);
  // Sorted keys: "a" first, ASCII escaped value for "é"
  ASSERT_NE(strstr(s, "\"a\": 1, \"b\": \"\\u00e9\""), nullptr);
}
