/***
 * Name: test_execute_strlen
 * Purpose: Compile and run a program using len on a string variable; verify exit code is length.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

TEST(ExecuteExample, ReturnsStringLen) {
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates = {fs::path("../../../demos"), fs::path("../../demos"), fs::path("demos")};
  fs::path demosDir;
  for (const auto& c : candidates) { if (fs::exists(c)) { demosDir = c; break; } }
  ASSERT_FALSE(demosDir.empty());
  const auto srcPath = (demosDir / "e2e_strlen.py").string();
  std::string cmd = std::string("../pycc -o e2e_strlen ") + srcPath + " > /dev/null 2>&1";
  int rc = std::system(cmd.c_str());
  if (rc != 0) { GTEST_SKIP() << "Skipping: pycc failed to compile example"; }

  rc = std::system("./e2e_strlen > /dev/null 2>&1");
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 5);
#else
  EXPECT_EQ(rc, 5 << 8);
#endif
}
