/***
 * Name: test_eval_dynamic_rejected (e2e)
 * Purpose: Ensure eval/exec with runtime-dynamic strings are rejected by the compiler.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>

TEST(EvalDynamicE2E, RejectsEvalDynamic) {
  const char* srcPath = "dyn_eval.py";
  {
    std::ofstream out(srcPath);
    out << "def main() -> int:\n";
    out << "  s = \"1+2\"\n";
    out << "  x = eval(s)\n";
    out << "  return 0\n";
  }
  const char* cmd = "../pycc --color=never -o out dyn_eval.py 2> dyn_eval.txt";
  int rc = std::system(cmd);
  ASSERT_NE(rc, 0);
}

TEST(EvalDynamicE2E, RejectsExecDynamic) {
  const char* srcPath = "dyn_exec.py";
  {
    std::ofstream out(srcPath);
    out << "def main() -> int:\n";
    out << "  s = \"print(1)\"\n";
    out << "  exec(s)\n";
    out << "  return 0\n";
  }
  const char* cmd = "../pycc --color=never -o out dyn_exec.py 2> dyn_exec.txt";
  int rc = std::system(cmd);
  ASSERT_NE(rc, 0);
}

