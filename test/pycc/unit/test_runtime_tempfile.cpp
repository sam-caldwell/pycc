/***
 * Name: test_runtime_tempfile
 * Purpose: Verify tempfile.gettempdir/mkdtemp/mkstemp runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"
#include <sys/stat.h>

using namespace pycc::rt;

static bool path_exists(const char* p) {
  struct stat st{}; return ::stat(p, &st) == 0;
}

TEST(RuntimeTempfile, GetTempDirAndCreate) {
  gc_reset_for_tests();
  void* d = tempfile_gettempdir();
  ASSERT_NE(d, nullptr);
  ASSERT_GT(string_len(d), 0u);
  void* nd = tempfile_mkdtemp();
  ASSERT_NE(nd, nullptr);
  std::string dir(string_data(nd), string_len(nd));
  ASSERT_TRUE(path_exists(dir.c_str()));
  // Create temp file
  void* f = tempfile_mkstemp();
  ASSERT_NE(f, nullptr);
  // f is a list [fd,path]
  void* path = list_get(f, 1);
  ASSERT_NE(path, nullptr);
  std::string fp(string_data(path), string_len(path));
  ASSERT_TRUE(path_exists(fp.c_str()));
  // cleanup
  (void)os_remove(fp.c_str());
  (void)os_remove(dir.c_str());
}

