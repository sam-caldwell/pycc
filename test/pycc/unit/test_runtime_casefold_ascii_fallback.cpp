/***
 * Name: test_runtime_casefold_ascii_fallback
 * Purpose: Ensure ASCII-only fallback casefold lowercases A-Z when ICU is not enabled.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

#ifndef PYCC_WITH_ICU
TEST(RuntimeUnicodeFallback, CasefoldAscii) {
  gc_reset_for_tests();
  void* s = string_from_cstr("AbCé");
  void* cf = string_casefold(s);
  ASSERT_NE(cf, nullptr);
  // Non-ASCII 'é' remains as-is in fallback; ASCII letters lowered
  std::string out(string_data(cf), string_len(cf));
  EXPECT_EQ(out, std::string("abcé"));
}
#endif

