/***
 * Name: test_runtime_binascii
 * Purpose: Verify binascii.hexlify/unhexlify runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeBinascii, HexlifyUnhexlify) {
  gc_reset_for_tests();
  void* s = string_from_cstr("hi");
  void* h = binascii_hexlify(s);
  ASSERT_NE(h, nullptr);
  std::string hs(reinterpret_cast<const char*>(bytes_data(h)), bytes_len(h));
  EXPECT_EQ(hs, std::string("6869"));
  void* b = binascii_unhexlify(h);
  ASSERT_EQ(bytes_len(b), 2u);
  const unsigned char* bd = bytes_data(b);
  EXPECT_EQ(bd[0], 'h'); EXPECT_EQ(bd[1], 'i');
}

