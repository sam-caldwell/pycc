/***
 * Name: test_runtime_binascii
 * Purpose: Verify binascii.hexlify/unhexlify runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

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

TEST(RuntimeBinascii, UppercasePrefixAndBytesInput) {
  gc_reset_for_tests();
  // Uppercase hex with 0x prefix should decode
  void* hx = string_from_cstr("0x4869");
  void* b = binascii_unhexlify(hx);
  ASSERT_EQ(bytes_len(b), 2u);
  EXPECT_EQ(bytes_data(b)[0], 'H');
  EXPECT_EQ(bytes_data(b)[1], 'i');

  // bytes input to hexlify should work
  unsigned char raw[3] = {0xff, 0x00, 0x7f};
  void* bb = bytes_new(raw, sizeof(raw));
  void* hx2 = binascii_hexlify(bb);
  ASSERT_NE(hx2, nullptr);
  std::string s(reinterpret_cast<const char*>(bytes_data(hx2)), bytes_len(hx2));
  EXPECT_EQ(s, std::string("ff007f"));
}

