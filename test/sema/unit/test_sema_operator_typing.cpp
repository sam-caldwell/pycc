/***
 * Name: test_sema_operator_typing
 * Purpose: Ensure Sema types operator.* and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "op.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaOperator, AcceptsNumeric) {
  const char* src = R"PY(
def main() -> int:
  a = operator.add(1, 2)
  b = operator.sub(3.0, 1)
  c = operator.mul(2, 4.0)
  d = operator.truediv(1, 2)
  e = operator.neg(5)
  f = operator.eq(1, 1)
  g = operator.lt(1, 2)
  h = operator.not_(0)
  i = operator.truth(1)
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaOperator, RejectsWrongArityAndType) {
  const char* src1 = R"PY(
def main() -> int:
  a = operator.add("x", 1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = operator.not_(1, 2)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

