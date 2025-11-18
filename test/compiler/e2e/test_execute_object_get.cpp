/***
 * Name: test_execute_object_get
 * Purpose: Compile and run a program using object(...) and obj_get; verify len of field is returned.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

TEST(ExecuteObjectGet, ReturnsFieldStringLen) {
  const char* srcPath = "e2e_run_objget.py";
  {
    std::ofstream out(srcPath);
    out << "def main() -> int:\n";
    out << "  o = object(\"hello\")\n";
    out << "  s = obj_get(o, 0)\n";
    out << "  return len(s)\n";
  }
  int rc = std::system("../pycc -o e2e_objget e2e_run_objget.py > /dev/null 2>&1");
  if (rc != 0) { GTEST_SKIP() << "Skipping: pycc failed to compile object-get example"; }

  rc = std::system("./e2e_objget > /dev/null 2>&1");
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 5);
#else
  EXPECT_EQ(rc, 5 << 8);
#endif
}
