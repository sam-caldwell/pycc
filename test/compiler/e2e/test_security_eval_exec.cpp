/***
 * Name: test_security_eval_exec (e2e)
 * Purpose: Ensure CLI/compiler accepts literal-only eval/exec at compile time.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include <string>



TEST(SecurityE2E, AcceptsLiteralEval) {
  const char* srcPath = "sec_eval.py";
  {
    std::ofstream out(srcPath);
    out << "def main() -> int:\n";
    out << "  x = eval(\"1+2\")\n";
    out << "  return 0\n";
  }
  const char* cmd = "../pycc --color=never -o out sec_eval.py 2> sec_eval.txt";
  int rc = std::system(cmd);
  ASSERT_EQ(rc, 0);
}

TEST(SecurityE2E, AcceptsLiteralExec) {
  const char* srcPath = "sec_exec.py";
  {
    std::ofstream out(srcPath);
    out << "def main() -> int:\n";
    out << "  exec(\"print(1)\")\n";
    out << "  return 0\n";
  }
  const char* cmd = "../pycc --color=never -o out sec_exec.py 2> sec_exec.txt";
  int rc = std::system(cmd);
  ASSERT_EQ(rc, 0);
}
