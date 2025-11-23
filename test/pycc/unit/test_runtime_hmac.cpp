/***
 * Name: test_runtime_hmac
 * Purpose: Verify hmac.digest runtime shim (deterministic subset).
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeHmac, DigestLengthsAndDeterminism) {
  gc_reset_for_tests();
  void* d1 = hmac_digest(string_from_cstr("key"), string_from_cstr("msg"), string_from_cstr("sha256"));
  ASSERT_NE(d1, nullptr);
  EXPECT_EQ(bytes_len(d1), 32u);
  void* d2 = hmac_digest(string_from_cstr("key"), string_from_cstr("msg"), string_from_cstr("md5"));
  ASSERT_NE(d2, nullptr);
  EXPECT_EQ(bytes_len(d2), 16u);
  void* d3 = hmac_digest(string_from_cstr("key"), string_from_cstr("msg"), string_from_cstr("sha256"));
  ASSERT_EQ(bytes_len(d3), 32u);
  // Deterministic for same inputs
  const unsigned char* a = bytes_data(d1); const unsigned char* b = bytes_data(d3);
  for (std::size_t i=0;i<bytes_len(d1);++i) { EXPECT_EQ(a[i], b[i]); }
}

