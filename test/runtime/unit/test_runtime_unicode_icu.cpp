/***
 * Name: test_runtime_unicode_icu
 * Purpose: ICU-backed normalization and casefold tests (guarded by PYCC_WITH_ICU).
 */
#include <gtest/gtest.h>
#include "runtime/All.h"
#include <cstring>

using namespace pycc::rt;

#ifdef PYCC_WITH_ICU
TEST(RuntimeUnicodeICU, NormalizationAndCasefold) {
  gc_reset_for_tests();
  // NFC("e\u0301") == "\xC3\xA9" (é)
  const unsigned char comb[] = { 'e', 0xCCu, 0x81u };
  void* s = string_new(reinterpret_cast<const char*>(comb), sizeof(comb));
  void* n = string_normalize(s, NormalizationForm::NFC);
  const char* d = string_data(n);
  ASSERT_EQ(string_len(n), 2u);
  EXPECT_EQ(static_cast<unsigned char>(d[0]), 0xC3u);
  EXPECT_EQ(static_cast<unsigned char>(d[1]), 0xA9u);

  // Casefold("Straße") -> "strasse"
  const unsigned char stra[] = { 'S','t','r','a', 0xC3u, 0x9Fu, 'e' };
  void* s2 = string_new(reinterpret_cast<const char*>(stra), sizeof(stra));
  void* cf = string_casefold(s2);
  ASSERT_EQ(string_len(cf), 7u);
  EXPECT_EQ(std::memcmp(string_data(cf), "strasse", 7), 0);
}
#endif

