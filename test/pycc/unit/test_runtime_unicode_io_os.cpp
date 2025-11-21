/***
 * Name: test_runtime_unicode_io_os
 * Purpose: Validate utf8 helpers and basic I/O and OS interop.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"
#include <cstdio>
#include <cstdlib>

using namespace pycc::rt;

TEST(RuntimeUnicode, Utf8Validation) {
  const char* valid = "Hello, \xF0\x9F\x98\x80"; // "Hello, 😀"
  EXPECT_TRUE(utf8_is_valid(valid, 10));

  const unsigned char invalid_seq[] = {0xC0, 0xAF, 0x00}; // overlong '/'
  EXPECT_FALSE(utf8_is_valid(reinterpret_cast<const char*>(invalid_seq), 2));
}

TEST(RuntimeIOOS, ReadWriteFileAndGetenv) {
  gc_reset_for_tests();
  const char* path = "_rt_tmp_test.txt";
  void* s = string_from_cstr("abc\n\0def"); // includes NUL in the middle
  ASSERT_TRUE(io_write_file(path, s));
  void* read = io_read_file(path);
  ASSERT_NE(read, nullptr);
  EXPECT_EQ(string_len(read), string_len(s));
  EXPECT_EQ(std::memcmp(string_data(read), string_data(s), string_len(s)), 0);
  std::remove(path);

  // getenv roundtrip using a temporary var
#if defined(_WIN32)
  _putenv("PYCC_TEST_ENV=xyz");
#else
  setenv("PYCC_TEST_ENV", "xyz", 1);
#endif
  void* envval = os_getenv("PYCC_TEST_ENV");
  ASSERT_NE(envval, nullptr);
  EXPECT_STREQ(string_data(envval), "xyz");
}
