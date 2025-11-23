/***
 * Name: test_runtime_unicodedata
 * Purpose: Verify unicodedata.normalize runtime shim.
 */
#include <gtest/gtest.h>
#include <string>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeUnicodedata, NormalizeValidForms) {
  gc_reset_for_tests();
  void* s = string_from_cstr("cafe");
  const char* forms[] = {"NFC","NFD","NFKC","NFKD"};
  for (const char* f : forms) {
    void* out = unicodedata_normalize(string_from_cstr(f), s);
    ASSERT_NE(out, nullptr);
    std::string os(string_data(out), string_len(out));
    EXPECT_EQ(os, std::string("cafe"));
  }
}

TEST(RuntimeUnicodedata, NormalizeInvalidFormRaises) {
  gc_reset_for_tests();
  void* s = string_from_cstr("x");
  void* out = unicodedata_normalize(string_from_cstr("BAD"), s);
  (void)out;
  ASSERT_TRUE(rt_has_exception());
  void* exc = rt_current_exception();
  ASSERT_NE(exc, nullptr);
  EXPECT_STREQ(string_data(rt_exception_type(exc)), "ValueError");
  rt_clear_exception();
}

