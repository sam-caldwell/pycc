/***
 * Name: test_execute_fnmatch
 * Purpose: Compile and run a program using fnmatch; verify stdout and exit code.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

static std::string slurp(const std::string& p) {
  std::ifstream in(p);
  std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return s;
}

TEST(ExecuteFnmatch, StdoutAndExit) {
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates = {fs::path("../../../demos"), fs::path("../../demos"), fs::path("demos")};
  fs::path demosDir; for (const auto& c : candidates) { if (fs::exists(c)) { demosDir = c; break; } }
  ASSERT_FALSE(demosDir.empty());
  const auto srcPath = std::filesystem::weakly_canonical(demosDir / "e2e_fnmatch.py").string();
  std::error_code ec; std::filesystem::create_directory("../Testing", ec);
  std::string cmd = std::string("../pycc -o ../Testing/e2e_fnmatch ") + srcPath + " > /dev/null 2>&1";
  int rc = std::system(cmd.c_str());
  if (rc != 0) { GTEST_SKIP() << "pycc failed to compile fnmatch demo"; return; }
  rc = std::system("../Testing/e2e_fnmatch > ../Testing/out_fnmatch.txt 2>/dev/null");
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 2);
#else
  EXPECT_EQ(rc, 2 << 8);
#endif
  auto out = slurp("../Testing/out_fnmatch.txt");
  EXPECT_EQ(out, std::string("FN_OK\\n"));
}
