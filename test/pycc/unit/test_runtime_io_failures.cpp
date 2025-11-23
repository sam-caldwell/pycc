/***
 * Name: test_runtime_io_failures
 * Purpose: Cover file I/O failure paths without OS dependency.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeIOFailures, ReadFileNullAndMissing) {
  EXPECT_EQ(io_read_file(nullptr), nullptr);
  // Try a likely-missing file name
  EXPECT_EQ(io_read_file("__pycc_file_does_not_exist__"), nullptr);
}

TEST(RuntimeIOFailures, WriteFileNullPathRejected) {
  void* s = string_from_cstr("x");
  EXPECT_FALSE(io_write_file(nullptr, s));
}

TEST(RuntimeIOFailures, WriteFileEmptyPathRejected) {
  void* s = string_from_cstr("x");
  EXPECT_FALSE(io_write_file("", s));
}

