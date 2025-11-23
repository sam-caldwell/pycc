/***
 * Name: test_runtime_base64
 * Purpose: Verify base64.b64encode/b64decode runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeBase64, EncodeDecodeString) {
  gc_reset_for_tests();
  void* s = string_from_cstr("hello");
  void* enc = base64_b64encode(s);
  ASSERT_NE(enc, nullptr);
  std::string es(reinterpret_cast<const char*>(bytes_data(enc)), bytes_len(enc));
  EXPECT_EQ(es, std::string("aGVsbG8="));
  void* dec = base64_b64decode(enc);
  std::string ds(reinterpret_cast<const char*>(bytes_data(dec)), bytes_len(dec));
  EXPECT_EQ(ds, std::string("hello"));
}

TEST(RuntimeBase64, EncodeDecodeBytes) {
  gc_reset_for_tests();
  const unsigned char raw[3] = {0x00, 0xFF, 0x10};
  void* b = bytes_new(raw, 3);
  void* enc = base64_b64encode(b);
  ASSERT_NE(enc, nullptr);
  void* dec = base64_b64decode(enc);
  ASSERT_EQ(bytes_len(dec), 3u);
  const unsigned char* d = bytes_data(dec);
  EXPECT_EQ(d[0], 0x00); EXPECT_EQ(d[1], 0xFF); EXPECT_EQ(d[2], 0x10);
}

