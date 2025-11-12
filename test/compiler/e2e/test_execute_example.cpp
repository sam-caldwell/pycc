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
  const char* srcPath = "e2e_run_tmp.py";
  {
    std::ofstream out(srcPath);
    out << "def main() -> int:\n";
    out << "  return 5\n";
  }
  // Build the program
  int rc = std::system("../pycc -o e2e_app e2e_run_tmp.py > /dev/null 2>&1");
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
