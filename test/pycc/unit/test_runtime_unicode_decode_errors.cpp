/***
 * Name: test_runtime_unicode_decode_errors
 * Purpose: Ensure bytes_decode error paths and replacement behavior are correct; also unknown encodings.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"
#include <cstring>

using namespace pycc::rt;

TEST(RuntimeUnicodeDecode, Utf8InvalidStrictRaises) {
  gc_reset_for_tests();
  const unsigned char bad[] = {0xC0u, 0xAFu}; // overlong '/'
  void* b = bytes_new(bad, sizeof(bad));
  void* s = bytes_decode(b, "utf-8", "strict");
  EXPECT_EQ(s, nullptr);
  ASSERT_TRUE(rt_has_exception());
  void* exc = rt_current_exception();
  ASSERT_STREQ(string_data(rt_exception_type(exc)), "UnicodeDecodeError");
  rt_clear_exception();
}

TEST(RuntimeUnicodeDecode, Utf8InvalidReplaceRepaired) {
  gc_reset_for_tests();
  const unsigned char bad[] = { 'a', 0xC0u, 0xAFu, 'b' };
  void* b = bytes_new(bad, sizeof(bad));
  void* s = bytes_decode(b, "utf-8", "replace");
  ASSERT_NE(s, nullptr);
  // Replacement character U+FFFD encoded in UTF-8 is 0xEF 0xBF 0xBD
  const char* out = string_data(s);
  const std::size_t n = string_len(s);
  ASSERT_GE(n, 4u);
  bool found = false;
  for (std::size_t i = 0; i + 2 < n; ++i) {
    if ((unsigned char)out[i] == 0xEFu && (unsigned char)out[i+1] == 0xBFu && (unsigned char)out[i+2] == 0xBDu) { found = true; break; }
  }
  EXPECT_TRUE(found);
}

TEST(RuntimeUnicodeDecode, AsciiInvalidStrictRaises) {
  gc_reset_for_tests();
  const unsigned char raw[] = { 'x', 0xFFu };
  void* b = bytes_new(raw, sizeof(raw));
  void* s = bytes_decode(b, "ascii", "strict");
  EXPECT_EQ(s, nullptr);
  ASSERT_TRUE(rt_has_exception());
  void* exc = rt_current_exception();
  ASSERT_STREQ(string_data(rt_exception_type(exc)), "UnicodeDecodeError");
  rt_clear_exception();
}

TEST(RuntimeUnicodeDecode, AsciiInvalidReplaceUsesQuestionMark) {
  gc_reset_for_tests();
  const unsigned char raw[] = { 'x', 0xFFu };
  void* b = bytes_new(raw, sizeof(raw));
  void* s = bytes_decode(b, "ascii", "replace");
  ASSERT_NE(s, nullptr);
  ASSERT_EQ(string_len(s), 2u);
  EXPECT_EQ(std::memcmp(string_data(s), "x?", 2), 0);
}

TEST(RuntimeUnicodeDecode, UnknownEncodingRaisesLookupError) {
  gc_reset_for_tests();
  const unsigned char raw[] = { 'a' };
  void* b = bytes_new(raw, sizeof(raw));
  void* s = bytes_decode(b, "latin-1", "strict");
  EXPECT_EQ(s, nullptr);
  ASSERT_TRUE(rt_has_exception());
  void* exc = rt_current_exception();
  ASSERT_STREQ(string_data(rt_exception_type(exc)), "LookupError");
  rt_clear_exception();
}

