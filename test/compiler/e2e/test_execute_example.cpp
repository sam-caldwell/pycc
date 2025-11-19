/***
 * Name: test_execute_example
 * Purpose: Compile and run a minimal program; verify exit status 5.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

TEST(ExecuteExample, ReturnsFive) {
  // Use demos/minimal.py
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates = {fs::path("../../../demos"), fs::path("../../demos"), fs::path("demos")};
  fs::path demosDir;
  for (const auto& c : candidates) { if (fs::exists(c)) { demosDir = c; break; } }
  ASSERT_FALSE(demosDir.empty());
  const auto srcPath = (demosDir / "minimal.py").string();
  // Build the program
  std::string cmd = std::string("../pycc -o e2e_app ") + srcPath + " > /dev/null 2>&1";
  int rc = std::system(cmd.c_str());
  ASSERT_EQ(rc, 0) << "pycc failed to compile example";

  // Execute the result and check exit status
  rc = std::system("./e2e_app > /dev/null 2>&1");
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 5);
#else
  // Fallback: on non-POSIX, std::system() returns implementation-defined.
  // We at least assert non-zero status shift behavior: rc should be 5<<8.
  EXPECT_EQ(rc, 5 << 8);
#endif
}
