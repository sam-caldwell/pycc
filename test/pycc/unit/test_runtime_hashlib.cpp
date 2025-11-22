/***
 * Name: test_runtime_hashlib
 * Purpose: Verify hashlib.sha256/md5 runtime shims (subset, deterministic).
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeHashlib, DeterministicHexLengths) {
  gc_reset_for_tests();
  void* s = string_from_cstr("hello");
  void* d1 = hashlib_sha256(s);
  void* d2 = hashlib_sha256(s);
  ASSERT_NE(d1, nullptr); ASSERT_NE(d2, nullptr);
  std::string h1(string_data(d1), string_len(d1));
  std::string h2(string_data(d2), string_len(d2));
  EXPECT_EQ(h1.size(), 64u);
  EXPECT_EQ(h2.size(), 64u);
  EXPECT_EQ(h1, h2);
  void* m1 = hashlib_md5(s);
  ASSERT_NE(m1, nullptr);
  std::string hm(string_data(m1), string_len(m1));
  EXPECT_EQ(hm.size(), 32u);
  // different input yields different digest
  void* s2 = string_from_cstr("world");
  void* d3 = hashlib_sha256(s2);
  ASSERT_NE(d3, nullptr);
  std::string h3(string_data(d3), string_len(d3));
  EXPECT_NE(h1, h3);
}

