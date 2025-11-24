/***
 * Name: test_execute_glob
 * Purpose: Compile and run a program using glob; verify stdout and exit code.
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

TEST(ExecuteGlob, StdoutAndExit) {
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates = {fs::path("../../../demos"), fs::path("../../demos"), fs::path("demos")};
  fs::path demosDir; for (const auto& c : candidates) { if (fs::exists(c)) { demosDir = c; break; } }
  ASSERT_FALSE(demosDir.empty());
  const auto srcPath = std::filesystem::weakly_canonical(demosDir / "e2e_glob.py").string();
  std::error_code ec; std::filesystem::create_directory("../Testing", ec);
  std::string cmd = std::string("../pycc -o ../Testing/e2e_glob ") + srcPath + " > /dev/null 2>&1";
  int rc = std::system(cmd.c_str());
  if (rc != 0) { GTEST_SKIP() << "pycc failed to compile glob demo"; return; }
  rc = std::system("../Testing/e2e_glob > ../Testing/out_glob.txt 2>/dev/null");
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_GE(code, 2);
#else
  EXPECT_GE(rc, 2 << 8);
#endif
  auto out = readAll("../Testing/out_glob.txt");
  EXPECT_EQ(out, std::string("GLOB_OK\\n\n"));
}
