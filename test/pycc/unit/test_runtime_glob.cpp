/***
 * Name: test_runtime_glob
 * Purpose: Verify glob.glob/iglob/escape runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeGlob, GlobAndEscape) {
  gc_reset_for_tests();
  // Prepare files
  const char* dname = "_glob_tmp";
  (void)os_remove(dname);
  ASSERT_TRUE(os_mkdir(dname, 0700));
  const char* f1 = "_glob_tmp/a.txt";
  const char* f2 = "_glob_tmp/b.cpp";
  (void)io_write_file(f1, string_from_cstr("x"));
  (void)io_write_file(f2, string_from_cstr("y"));

  void* patTxt = string_from_cstr("_glob_tmp/*.txt");
  void* res = glob_glob(patTxt);
  ASSERT_NE(res, nullptr);
  ASSERT_GE(list_len(res), 1u);

  void* patCpp = string_from_cstr("_glob_tmp/*.cpp");
  void* res2 = glob_iglob(patCpp);
  ASSERT_NE(res2, nullptr);
  ASSERT_GE(list_len(res2), 1u);

  void* esc = glob_escape(string_from_cstr("a*b?"));
  ASSERT_NE(esc, nullptr);
  ASSERT_GT(string_len(esc), 0u);

  // cleanup
  (void)os_remove(f1);
  (void)os_remove(f2);
  (void)os_remove(dname);
}

