/***
 * Name: test_runtime_pathlib
 * Purpose: Validate pathlib runtime shims for cross-platform behavior.
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include "runtime/Runtime.h"

using namespace pycc::rt;

static std::string toStdString(void* s) {
  const char* d = string_data(s); std::size_t n = string_len(s);
  return std::string(d ? d : "", d ? n : 0);
}

static std::filesystem::path toPath(void* s) {
  auto ss = toStdString(s);
  std::u8string u8(reinterpret_cast<const char8_t*>(ss.data()), reinterpret_cast<const char8_t*>(ss.data()) + ss.size());
  return std::filesystem::path(u8);
}

TEST(RuntimePathlib, CwdHomeNonEmpty) {
  void* c = pathlib_cwd();
  ASSERT_NE(c, nullptr);
  auto cwd = std::filesystem::current_path();
  EXPECT_EQ(toPath(c), cwd);
  void* h = pathlib_home();
  ASSERT_NE(h, nullptr);
  EXPECT_NE(toStdString(h).size(), 0u);
}

TEST(RuntimePathlib, JoinParentBasenameSuffixStem) {
  void* j = pathlib_join2(string_from_cstr("a"), string_from_cstr("b.txt"));
  auto jp = toPath(j);
  EXPECT_EQ(jp, std::filesystem::path("a") / "b.txt");
  void* p = pathlib_parent(j);
  EXPECT_EQ(toPath(p), std::filesystem::path("a"));
  void* bn = pathlib_basename(j);
  EXPECT_EQ(toStdString(bn), std::string("b.txt"));
  void* sx = pathlib_suffix(j);
  EXPECT_EQ(toStdString(sx), std::string(".txt"));
  void* st = pathlib_stem(j);
  EXPECT_EQ(toStdString(st), std::string("b"));
}

TEST(RuntimePathlib, WithNameAndSuffix) {
  void* p = string_from_cstr("a/b.txt");
  void* wn = pathlib_with_name(p, string_from_cstr("c.log"));
  EXPECT_EQ(toPath(wn), std::filesystem::path("a") / "c.log");
  void* ws = pathlib_with_suffix(p, string_from_cstr(".log"));
  EXPECT_EQ(toPath(ws), std::filesystem::path("a") / "b.log");
}

TEST(RuntimePathlib, ExistsIsFileIsDirMkdirRenameUnlinkRmdir) {
  // Use a unique temp dir under CWD
  auto base = std::filesystem::current_path() / "test_pathlib_tmp";
  std::error_code ec; std::filesystem::remove_all(base, ec);
  auto nested = base / "foo" / "bar";
  void* n = string_from_cstr(nested.generic_string().c_str());
  EXPECT_TRUE(pathlib_mkdir(n, 0777, 1, 1));
  EXPECT_TRUE(pathlib_exists(n));
  EXPECT_TRUE(pathlib_is_dir(n));
  // Create a file and test file ops
  auto f = base / "file.txt";
  {
    auto s = f; auto dir = s.parent_path(); std::filesystem::create_directories(dir, ec);
    FILE* fp = std::fopen(f.string().c_str(), "wb"); ASSERT_NE(fp, nullptr); std::fputs("hi", fp); std::fclose(fp);
  }
  void* fs = string_from_cstr(f.generic_string().c_str());
  EXPECT_TRUE(pathlib_exists(fs));
  EXPECT_TRUE(pathlib_is_file(fs));
  // Rename
  auto f2 = base / "file2.txt";
  void* f2s = string_from_cstr(f2.generic_string().c_str());
  EXPECT_TRUE(pathlib_rename(fs, f2s));
  EXPECT_FALSE(pathlib_exists(fs));
  EXPECT_TRUE(pathlib_exists(f2s));
  // Unlink and cleanup
  EXPECT_TRUE(pathlib_unlink(f2s));
  EXPECT_TRUE(pathlib_rmdir(n)); // removes deepest dir (bar)
  // Remove parent chain for cleanliness
  std::filesystem::remove_all(base, ec);
}

TEST(RuntimePathlib, PartsResolveAbsoluteAsUriPosixMatch) {
  auto ex = std::filesystem::path("a") / "b" / "c.txt";
  void* p = string_from_cstr(ex.generic_string().c_str());
  void* parts = pathlib_parts(p);
  // parts is a list; verify len == number of elements when iterating with std::filesystem
  size_t count = 0; for (auto it = ex.begin(); it != ex.end(); ++it) ++count;
  EXPECT_EQ(list_len(parts), count);
  // resolve/absolute return absolute paths
  void* ab = pathlib_absolute(p);
  EXPECT_TRUE(toPath(ab).is_absolute());
  void* rv = pathlib_resolve(p);
  EXPECT_TRUE(toPath(rv).is_absolute());
  // as_posix uses '/'
  void* pos = pathlib_as_posix(p);
  auto s = toStdString(pos);
  EXPECT_NE(s.find('/'), std::string::npos);
  // as_uri begins with file://
  void* cur = pathlib_cwd();
  void* uri = pathlib_as_uri(cur);
  auto us = toStdString(uri);
#if defined(_WIN32)
  ASSERT_TRUE(us.rfind("file:///", 0) == 0);
#else
  ASSERT_TRUE(us.rfind("file://", 0) == 0);
#endif
  // match against basename
  void* nm = string_from_cstr("file.txt");
  void* pat = string_from_cstr("file*.txt");
  EXPECT_TRUE(pathlib_match(nm, pat));
  void* pat2 = string_from_cstr("*.log");
  EXPECT_FALSE(pathlib_match(nm, pat2));
}
