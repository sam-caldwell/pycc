/***
 * Name: test_diagnostics_context_scoping
 * Purpose: E2E verify scoping errors print diagnostics with context lines.
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

TEST(DiagnosticsContextScoping, NonlocalMissingHasContext) {
  const char* srcPath = "../Testing/diag_scope_nonlocal.py";
  {
    std::ofstream out(srcPath);
    out << "def f() -> int\n";
    out << "  nonlocal a\n"; // invalid: no enclosing binding
    out << "  return 0\n";
  }
  const char* cmd = "../pycc --color=never --diag-context=2 -o ../Testing/diag_out_nonlocal ../Testing/diag_scope_nonlocal.py 2> ../Testing/diag_scope_nonlocal.txt";
  int rc = std::system(cmd);
  ASSERT_NE(rc, 0);
  auto diag = readFile("../Testing/diag_scope_nonlocal.txt");
  ASSERT_NE(diag.find("diag_scope_nonlocal.py:"), std::string::npos);
  ASSERT_NE(diag.find("error:"), std::string::npos);
  ASSERT_NE(diag.find("nonlocal"), std::string::npos);
  ASSERT_NE(diag.find("(context lines: 2)"), std::string::npos);
}

TEST(DiagnosticsContextScoping, GlobalReadWithoutDefHasContext) {
  const char* srcPath = "../Testing/diag_scope_global.py";
  {
    std::ofstream out(srcPath);
    out << "def f() -> int\n";
    out << "  global a\n";
    out << "  return a\n"; // invalid: reading global 'a' without module-level definition
  }
  const char* cmd = "../pycc --color=never --diag-context=2 -o ../Testing/diag_out_global ../Testing/diag_scope_global.py 2> ../Testing/diag_scope_global.txt";
  int rc = std::system(cmd);
  ASSERT_NE(rc, 0);
  auto diag = readFile("../Testing/diag_scope_global.txt");
  ASSERT_NE(diag.find("diag_scope_global.py:"), std::string::npos);
  ASSERT_NE(diag.find("error:"), std::string::npos);
  ASSERT_NE(diag.find("global"), std::string::npos);
  ASSERT_NE(diag.find("(context lines: 2)"), std::string::npos);
}

