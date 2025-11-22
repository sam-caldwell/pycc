/***
 * Name: test_runtime_unicode
 * Purpose: Verify code point-aware len/slice and basic encode/decode behaviors.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"
#include <cstring>

using namespace pycc::rt;

TEST(RuntimeUnicode, CodePointLenAndSlice) {
  gc_reset_for_tests();
  // U+1F4A9 PILE OF POO (4-byte UTF-8), U+00E9 'é' (2-byte UTF-8): a💩bé
  const unsigned char raw[] = { 'a', 0xF0u, 0x9Fu, 0x92u, 0xA9u, 'b', 0xC3u, 0xA9u };
  void* s = string_new(reinterpret_cast<const char*>(raw), sizeof(raw));
  ASSERT_EQ(string_charlen(s), 4u); // a, 💩, b, é
  void* mid = string_slice(s, 1, 2); // 💩b
  EXPECT_EQ(string_charlen(mid), 2u);
  // Byte content should match UTF-8 for [0xF0,0x9F,0x92,0xA9,'b']
  EXPECT_EQ(string_len(mid), 5u);
  const unsigned char expect[] = { 0xF0u, 0x9Fu, 0x92u, 0xA9u, 'b' };
  const char* data = string_data(mid);
  ASSERT_EQ(std::memcmp(data, expect, sizeof(expect)), 0);
}

TEST(RuntimeUnicode, EncodeDecodeUtf8Ascii) {
  gc_reset_for_tests();
  void* s = string_from_cstr("caf\xC3\xA9"); // café
  void* b = string_encode(s, "utf-8", "strict");
  ASSERT_NE(b, nullptr);
  // ascii replace should substitute non-ascii with '?'
  void* a = string_encode(s, "ascii", "replace");
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(bytes_len(a), 5u);
  const unsigned char* ad = bytes_data(a);
  EXPECT_EQ(std::memcmp(ad, "cafe?", 5), 0);
  // Decode back
  void* s2 = bytes_decode(b, "utf-8", "strict");
  ASSERT_NE(s2, nullptr);
  EXPECT_EQ(string_len(s2), string_len(s));
}
