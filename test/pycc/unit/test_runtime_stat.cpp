/***
 * Name: test_runtime_stat
 * Purpose: Verify stat_ifmt/isdir/isreg with real filesystem modes.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"
#include <sys/stat.h>

using namespace pycc::rt;

TEST(RuntimeStat, PredicatesWithFilesystem) {
  gc_reset_for_tests();
  const char* dname = "_stat_tmp_dir";
  (void)os_remove(dname);
  ASSERT_TRUE(os_mkdir(dname, 0700));
  struct stat st{};
  ASSERT_EQ(::stat(dname, &st), 0);
  int32_t m = static_cast<int32_t>(st.st_mode);
  EXPECT_TRUE(stat_isdir(m));
  EXPECT_FALSE(stat_isreg(m));
  const char* fname = "_stat_tmp_dir/f.txt";
  (void)io_write_file(fname, string_from_cstr("x"));
  ASSERT_EQ(::stat(fname, &st), 0);
  m = static_cast<int32_t>(st.st_mode);
  EXPECT_FALSE(stat_isdir(m));
  EXPECT_TRUE(stat_isreg(m));
  // cleanup
  (void)os_remove(fname);
  (void)os_remove(dname);
}

