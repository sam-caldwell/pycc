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

TEST(RuntimeGlob, RecursiveAndClasses) {
  gc_reset_for_tests();
  const char* base = "_glob_tmp2";
  (void)os_remove(base);
  ASSERT_TRUE(os_mkdir(base, 0700));
  ASSERT_TRUE(os_mkdir("_glob_tmp2/dir", 0700));
  ASSERT_TRUE(os_mkdir("_glob_tmp2/dir/sub", 0700));
  // Create files to exercise ** recursion, ?, and [] classes
  (void)io_write_file("_glob_tmp2/a.py", string_from_cstr("a"));
  (void)io_write_file("_glob_tmp2/dir/file.py", string_from_cstr("b"));
  (void)io_write_file("_glob_tmp2/dir/sub/file2.py", string_from_cstr("c"));
  (void)io_write_file("_glob_tmp2/dir/ab.txt", string_from_cstr("x"));
  (void)io_write_file("_glob_tmp2/dir/bb.txt", string_from_cstr("y"));

  // Recursive '**' should see all .py under base
  void* rec = glob_glob(string_from_cstr("_glob_tmp2/**/*.py"));
  ASSERT_NE(rec, nullptr);
  ASSERT_GE(list_len(rec), 3u);

  // '?' single-character match and class [ab] for first letter
  void* q = glob_glob(string_from_cstr("_glob_tmp2/dir/?b.txt"));
  ASSERT_NE(q, nullptr);
  ASSERT_EQ(list_len(q), 2u);

  // Class range and literal ']'
  (void)io_write_file("_glob_tmp2/dir/x]y.log", string_from_cstr("z"));
  void* cl = glob_glob(string_from_cstr("_glob_tmp2/dir/x[]]y.log"));
  ASSERT_NE(cl, nullptr);
  ASSERT_EQ(list_len(cl), 1u);

  // Escaped metacharacters should not act as wildcards
  void* escpat = glob_escape(string_from_cstr("_glob_tmp2/*?.py"));
  void* no = glob_glob(escpat);
  ASSERT_NE(no, nullptr);
  // It's unlikely a literal file named '*?.py' exists; expect 0
  ASSERT_EQ(list_len(no), 0u);

  // Cleanup
  (void)os_remove("_glob_tmp2/dir/x]y.log");
  (void)os_remove("_glob_tmp2/dir/ab.txt");
  (void)os_remove("_glob_tmp2/dir/bb.txt");
  (void)os_remove("_glob_tmp2/dir/sub/file2.py");
  (void)os_remove("_glob_tmp2/dir/file.py");
  (void)os_remove("_glob_tmp2/a.py");
  (void)os_remove("_glob_tmp2/dir/sub");
  (void)os_remove("_glob_tmp2/dir");
  (void)os_remove(base);
}
