/***
 * Name: test_diagnostics
 * Purpose: Verify diagnostics include source snippet and caret, and color control.
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

TEST(Diagnostics, SnippetAndCaret_NoColor) {
  const char* srcPath = "diag_tmp.py";
  {
    std::ofstream out(srcPath);
    out << "def main() -> int:\n";
    out << "  return x\n"; // undefined name
  }
  const char* cmd = "../pycc --color=never -o out diag_tmp.py 2> diag.txt";
  int rc = std::system(cmd);
  ASSERT_NE(rc, 0);
  auto diag = readFile("diag.txt");
  ASSERT_NE(diag.find("^"), std::string::npos);
}

TEST(Diagnostics, ColorAlways) {
  const char* srcPath = "diag_tmp2.py";
  {
    std::ofstream out(srcPath);
    out << "def main() -> int:\n";
    out << "  return x\n"; // undefined name
  }
  const char* cmd = "../pycc --color=always -o out diag_tmp2.py 2> diag2.txt";
  int rc = std::system(cmd);
  ASSERT_NE(rc, 0);
  auto diag = readFile("diag2.txt");
  // Look for ANSI red code
  ASSERT_NE(diag.find("\x1b[31merror:"), std::string::npos);
}
