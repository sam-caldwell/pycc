/***
 * Name: test_sema_expression_containers_typing
 * Purpose: Ensure tuple/list/object literal visitor paths are exercised after extraction.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "expr_containers.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaExprContainers, TupleAndListAndObject) {
  const char* src = R"PY(
def main() -> int:
  t = (1, 2, 3)
  l = [1, 2, 3]
  o = object('a', 'b')
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

