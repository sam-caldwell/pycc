/***
 * Name: test_runtime_abc
 * Purpose: Cover _abc registry and cache token behavior.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeABC, RegistryAndToken) {
  abc_reset();
  long long t0 = abc_get_cache_token();
  EXPECT_EQ(t0, 0);
  void* A = string_from_cstr("A");
  void* B = string_from_cstr("B");
  bool first = abc_register(A, B);
  EXPECT_TRUE(first);
  long long t1 = abc_get_cache_token();
  EXPECT_GT(t1, t0);
  EXPECT_TRUE(abc_is_registered(A, B));
  // Duplicate registration should be no-op
  bool second = abc_register(A, B);
  EXPECT_FALSE(second);
  EXPECT_EQ(abc_get_cache_token(), t1);
  // Invalidate cache bumps token
  abc_invalidate_cache();
  EXPECT_GT(abc_get_cache_token(), t1);
  // Reset clears registry and token
  abc_reset();
  EXPECT_EQ(abc_get_cache_token(), 0);
  EXPECT_FALSE(abc_is_registered(A, B));
}

