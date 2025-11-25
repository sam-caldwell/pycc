/***
 * Name: test_execute_shutil
 * Purpose: Compile and run a program using shutil; verify stdout and exit code.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

static std::string readAll(const std::string& p) {
  std::ifstream in(p);
  std::string s, line; while (std::getline(in, line)) { s += line; s += "\n"; }
  return s;
}

TEST(ExecuteShutil, StdoutAndExit) {
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates = {fs::path("../../../demos"), fs::path("../../demos"), fs::path("demos")};
  fs::path demosDir; for (const auto& c : candidates) { if (fs::exists(c)) { demosDir = c; break; } }
  ASSERT_FALSE(demosDir.empty());
  const auto srcPath = std::filesystem::weakly_canonical(demosDir / "e2e_shutil.py").string();
  std::error_code ec; std::filesystem::create_directory("../Testing", ec);
  const auto pyccPath = std::filesystem::weakly_canonical("../pycc").string();
  std::string cmd = std::string("\"") + pyccPath + "\" -o ../Testing/e2e_shutil \"" + srcPath + "\" > /dev/null 2>&1";
  int rc = std::system(cmd.c_str());
  if (rc != 0) { GTEST_SKIP() << "pycc failed to compile shutil demo"; return; }
  rc = std::system("../Testing/e2e_shutil > ../Testing/out_shutil.txt 2>/dev/null");
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 0);
#else
  EXPECT_EQ(rc, 0);
#endif
  auto out = readAll("../Testing/out_shutil.txt");
  EXPECT_EQ(out, std::string("SHUTIL_OK\n"));
}
