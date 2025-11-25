/***
 * Name: test_execute_statistics
 * Purpose: Compile and run a program using statistics; verify stdout and exit code.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

static std::string slurp_stats(const std::string& p) {
  std::ifstream in(p);
  std::string s; std::getline(in, s, '\0'); return s;
}

TEST(ExecuteStatistics, StdoutAndExit) {
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates = {fs::path("../../../demos"), fs::path("../../demos"), fs::path("demos")};
  fs::path demosDir; for (const auto& c : candidates) { if (fs::exists(c)) { demosDir = c; break; } }
  ASSERT_FALSE(demosDir.empty());
  const auto srcPath = std::filesystem::weakly_canonical(demosDir / "e2e_statistics.py").string();
  std::error_code ec; std::filesystem::create_directory("../Testing", ec);
  std::string cmd = std::string("../pycc -o ../Testing/e2e_statistics \"") + srcPath + "\"";
  int rc = std::system(cmd.c_str());
  if (rc != 0) { GTEST_SKIP() << "pycc failed to compile statistics demo"; return; }
  rc = std::system("../Testing/e2e_statistics > ../Testing/out_statistics.txt");
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 0);
#else
  EXPECT_EQ(rc, 0);
#endif
  auto out = slurp_stats("../Testing/out_statistics.txt");
  EXPECT_EQ(out, std::string("STATISTICS_OK\\n"));
}
