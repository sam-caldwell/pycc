/***
 * Name: test_runtime_secrets
 * Purpose: Verify secrets.token_* runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"
#include <cctype>

using namespace pycc::rt;

TEST(RuntimeSecrets, TokenBytesLen) {
  gc_reset_for_tests();
  void* b = secrets_token_bytes(16);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(bytes_len(b), 16u);
}

static bool is_urlsafe(const char* s, std::size_t n) {
  for (std::size_t i=0;i<n;++i) {
    char c = s[i];
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c=='-' || c=='_')) return false;
  }
  return true;
}

TEST(RuntimeSecrets, TokenHexAndUrlsafeFormat) {
  gc_reset_for_tests();
  void* h = secrets_token_hex(8);
  std::string hs(string_data(h), string_len(h));
  ASSERT_EQ(hs.size(), 16u);
  for (char c : hs) { ASSERT_TRUE(std::isxdigit(static_cast<unsigned char>(c))); }
  void* u = secrets_token_urlsafe(8);
  std::string us(string_data(u), string_len(u));
  ASSERT_GT(us.size(), 0u);
  ASSERT_TRUE(is_urlsafe(us.data(), us.size()));
}

