/***
 * Name: test_diagnostics_context
 * Purpose: E2E validate diagnostics formatting in pycc with context option.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include <string>

static std::string readFile(const std::string& p) {
  std::ifstream in(p);
  std::string all, line;
  while (std::getline(in, line)) { all += line; all += "\n"; }
  return all;
}

TEST(DiagnosticsContext, WithContextTwoLines) {
  const char* srcPath = "../Testing/diag_ctx.py";
  {
    std::ofstream out(srcPath);
    out << "def main() -> int\n";
    out << "  return x\n"; // undefined name to trigger diag
  }
  const char* cmd = "../pycc --color=never --diag-context=2 -o ../Testing/diagc_out ../Testing/diag_ctx.py 2> ../Testing/diagc.txt";
  int rc = std::system(cmd);
  ASSERT_NE(rc, 0);
  auto diag = readFile("../Testing/diagc.txt");
  ASSERT_NE(diag.find("diag_ctx.py:"), std::string::npos);
  ASSERT_NE(diag.find("error:"), std::string::npos);
  ASSERT_NE(diag.find("^\n"), std::string::npos);
  ASSERT_NE(diag.find("(context lines: 2)"), std::string::npos);
}
