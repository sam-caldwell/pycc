/***
 * Name: test_sema_expression_literals_typing
 * Purpose: Ensure literal visitor paths are exercised after extraction.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "expr_lit.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaExprLiterals, BasicLiterals) {
  const char* src = R"PY(
def main() -> int:
  a = 1
  b = True
  c = 3.14
  d = "hello"
  e = None
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

