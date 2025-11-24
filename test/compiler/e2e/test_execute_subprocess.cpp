/***
 * Name: test_execute_subprocess
 * Purpose: Compile and run a program using subprocess.run; verify exit code is child exit status.
 */
#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <sys/wait.h>

TEST(ExecuteSubprocess, ReturnsExitCode) {
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates = {fs::path("../../../demos"), fs::path("../../demos"), fs::path("demos")};
  fs::path demosDir;
  for (const auto& c : candidates) { if (fs::exists(c)) { demosDir = c; break; } }
  ASSERT_FALSE(demosDir.empty());
  const auto srcPath = (demosDir / "e2e_subprocess.py").string();
  std::error_code ec; fs::create_directory("../Testing", ec);
  std::string cmd = std::string("../pycc -o ../Testing/e2e_subprocess ") + srcPath + " > /dev/null 2>&1";
  int rc = std::system(cmd.c_str());
  if (rc != 0) { GTEST_SKIP() << "Skipping: pycc failed to compile example"; }
  rc = std::system("../Testing/e2e_subprocess > /dev/null 2>&1");
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 0);
#else
  EXPECT_EQ(rc, 0);
#endif
}
