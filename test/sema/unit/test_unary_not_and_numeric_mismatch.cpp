/***
 * Name: test_unary_not_and_numeric_mismatch
 * Purpose: Cover 'not' requires bool and direct int+float mismatch in Sema.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "not_num.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaUnaryNot, RequiresBool) {
  const char* src = R"PY(
def f() -> int:
  if not 1:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaNumericMismatch, IntPlusFloatFails) {
  const char* src = R"PY(
def f() -> int:
  return 1 + 1.0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

