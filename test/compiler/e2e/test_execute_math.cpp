/***
 * Name: test_execute_math
 * Purpose: Compile and run a program using math; verify exit status equals computed result.
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

TEST(ExecuteMath, ReturnsFour) {
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates = {fs::path("../../../demos"), fs::path("../../demos"), fs::path("demos")};
  fs::path demosDir;
  for (const auto& c : candidates) { if (fs::exists(c)) { demosDir = c; break; } }
  ASSERT_FALSE(demosDir.empty());
  const auto srcPath = (demosDir / "e2e_math.py").string();
  std::string cmd = std::string("../pycc -o e2e_math ") + srcPath + " > /dev/null 2>&1";
  int rc = std::system(cmd.c_str());
  ASSERT_EQ(rc, 0) << "pycc failed to compile e2e_math.py";

  rc = std::system("./e2e_math > /dev/null 2>&1");
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 4);
#else
  EXPECT_EQ(rc, 4 << 8);
#endif
}
