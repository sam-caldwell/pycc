/***
 * Name: test_runtime_os_path
 * Purpose: Verify os.path subset runtime shims.
 */
#include <gtest/gtest.h>
#include <string>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeOsPath, JoinSplitextDirBaseAbspath) {
  gc_reset_for_tests();
  void* p = os_path_join2(string_from_cstr("/tmp"), string_from_cstr("file.txt"));
  std::string ps(string_data(p), string_len(p));
  // Platform-independent check: endswith("/tmp/file.txt") or "\\tmp\\file.txt"
  ASSERT_TRUE(ps.find("file.txt") != std::string::npos);
  void* sp = os_path_splitext(p);
  ASSERT_EQ(list_len(sp), 2u);
  std::string root(string_data(list_get(sp,0)), string_len(list_get(sp,0)));
  std::string ext(string_data(list_get(sp,1)), string_len(list_get(sp,1)));
  EXPECT_EQ(ext, std::string(".txt"));
  void* dn = os_path_dirname(p);
  std::string dns(string_data(dn), string_len(dn));
  ASSERT_TRUE(dns.find("tmp") != std::string::npos);
  void* bn = os_path_basename(p);
  std::string bns(string_data(bn), string_len(bn));
  EXPECT_EQ(bns, std::string("file.txt"));
  void* ap = os_path_abspath(string_from_cstr("."));
  ASSERT_NE(ap, nullptr);
}

TEST(RuntimeOsPath, ExistsIsFileIsDirRoundtrip) {
  gc_reset_for_tests();
  // Use CWD as an existing directory
  void* cwd = os_getcwd();
  ASSERT_TRUE(os_path_exists(cwd));
  ASSERT_TRUE(os_path_isdir(cwd));
  ASSERT_FALSE(os_path_isfile(cwd));
  // Create a temp file in CWD
  void* tmp = os_path_join2(cwd, string_from_cstr("os_path_test_tmp.txt"));
  bool w = io_write_file(string_data(tmp), string_from_cstr("hello"));
  ASSERT_TRUE(w);
  ASSERT_TRUE(os_path_exists(tmp));
  ASSERT_TRUE(os_path_isfile(tmp));
  // Cleanup
  (void)os_remove(string_data(tmp));
}

