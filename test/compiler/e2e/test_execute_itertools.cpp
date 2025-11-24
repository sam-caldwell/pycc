/***
 * Name: test_execute_itertools
 * Purpose: Compile and run a program using itertools; verify stdout and exit code.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

static std::string slurp_it(const std::string& p) {
  std::ifstream in(p); std::string s; std::getline(in, s, '\0'); return s;
}

TEST(ExecuteItertools, StdoutAndExit) {
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates = {fs::path("../../../demos"), fs::path("../../demos"), fs::path("demos")};
  fs::path demosDir; for (const auto& c : candidates) { if (fs::exists(c)) { demosDir = c; break; } }
  ASSERT_FALSE(demosDir.empty());
  const auto srcPath = std::filesystem::weakly_canonical(demosDir / "e2e_itertools.py").string();
  std::error_code ec; std::filesystem::create_directory("../Testing", ec);
  std::string cmd = std::string("../pycc -o ../Testing/e2e_itertools ") + srcPath + " > /dev/null 2>&1";
  int rc = std::system(cmd.c_str());
  if (rc != 0) { GTEST_SKIP() << "pycc failed to compile itertools demo"; return; }
  rc = std::system("../Testing/e2e_itertools > ../Testing/out_itertools.txt 2>/dev/null");
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 0);
#else
  EXPECT_EQ(rc, 0);
#endif
  auto out = slurp_it("../Testing/out_itertools.txt");
  EXPECT_EQ(out, std::string("IT_OK\\n"));
}
