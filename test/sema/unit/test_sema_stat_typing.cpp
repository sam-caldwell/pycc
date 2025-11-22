/***
 * Name: test_sema_stat_typing
 * Purpose: Ensure Sema types stat functions and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "statm.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaStat, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = stat.S_IFMT(0)
  b = stat.S_ISDIR(0)
  c = stat.S_ISREG(0)
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaStat, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  a = stat.S_IFMT("x")
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
}

