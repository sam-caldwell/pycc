/***
 * Name: test_runtime_os_fs
 * Purpose: Exercise OS/FS helpers: getcwd, mkdir, rename, remove.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"
#include <cstdio>

using namespace pycc::rt;

TEST(RuntimeOSFS, CwdMkdirRenameRemove) {
  gc_reset_for_tests();
  void* cwd = os_getcwd();
  ASSERT_NE(cwd, nullptr);
  ASSERT_GT(string_len(cwd), 0u);

  const char* dname = "_rt_tmp_dir";
  (void)os_remove(dname); // cleanup if exists
  EXPECT_TRUE(os_mkdir(dname, 0700));
  const char* fname = "_rt_tmp_file.txt";
  void* s = string_from_cstr("x");
  ASSERT_TRUE(io_write_file(fname, s));
  const char* fname2 = "_rt_tmp_file2.txt";
  EXPECT_TRUE(os_rename(fname, fname2));
  EXPECT_TRUE(os_remove(fname2));
  // cleanup dir
  EXPECT_TRUE(os_remove(dname));
}

