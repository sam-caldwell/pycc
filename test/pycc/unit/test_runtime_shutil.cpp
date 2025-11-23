/***
 * Name: test_runtime_shutil
 * Purpose: Verify shutil.copyfile/copy runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeShutil, CopyFileAndCopy) {
  gc_reset_for_tests();
  const char* s1 = "_shutil_src.txt";
  const char* s2 = "_shutil_dst.txt";
  const char* s3 = "_shutil_dst2.txt";
  (void)os_remove(s1); (void)os_remove(s2); (void)os_remove(s3);
  ASSERT_TRUE(io_write_file(s1, string_from_cstr("abc")));
  void* p1 = string_from_cstr(s1);
  void* p2 = string_from_cstr(s2);
  void* p3 = string_from_cstr(s3);
  EXPECT_TRUE(shutil_copyfile(p1, p2));
  void* c2 = io_read_file(s2);
  ASSERT_NE(c2, nullptr);
  EXPECT_EQ(std::string(string_data(c2), string_len(c2)), std::string("abc"));
  EXPECT_TRUE(shutil_copy(p2, p3));
  void* c3 = io_read_file(s3);
  ASSERT_NE(c3, nullptr);
  EXPECT_EQ(std::string(string_data(c3), string_len(c3)), std::string("abc"));
  (void)os_remove(s1); (void)os_remove(s2); (void)os_remove(s3);
}

