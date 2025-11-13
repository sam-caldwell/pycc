/***
 * Name: test_execute_list_len
 * Purpose: Compile and run a program constructing a list, returning len(a)=3.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

TEST(ExecuteListLen, ReturnsThree) {
  const char* srcPath = "e2e_run_listlen.py";
  {
    std::ofstream out(srcPath);
    out << "def main() -> int:\n";
    out << "  a = [1,2,3]\n";
    out << "  return len(a)\n";
  }
  int rc = std::system("../pycc -o e2e_listlen e2e_run_listlen.py > /dev/null 2>&1");
  ASSERT_EQ(rc, 0) << "pycc failed to compile list-len example";

  rc = std::system("./e2e_listlen > /dev/null 2>&1");
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 3);
#else
  EXPECT_EQ(rc, 3 << 8);
#endif
}

