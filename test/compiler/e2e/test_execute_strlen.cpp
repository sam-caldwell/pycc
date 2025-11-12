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
  const char* srcPath = "e2e_run_strlen.py";
  {
    std::ofstream out(srcPath);
    out << "def main() -> int:\n";
    out << "  s = \"hello\"\n";
    out << "  return len(s)\n";
  }
  int rc = std::system("../pycc -o e2e_strlen e2e_run_strlen.py > /dev/null 2>&1");
  ASSERT_EQ(rc, 0) << "pycc failed to compile example";

  rc = std::system("./e2e_strlen > /dev/null 2>&1");
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 5);
#else
  EXPECT_EQ(rc, 5 << 8);
#endif
}

