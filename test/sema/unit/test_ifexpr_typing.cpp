/***
 * Name: test_ifexpr_typing
 * Purpose: Sema checks for if-expression condition type and arm type match.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "ifexpr.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaIfExpr, ConditionMustBeBool) {
  const char* src = R"PY(
def f() -> int:
  return (1 if 1 else 0)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaIfExpr, BranchesMustMatchType) {
  const char* src = R"PY(
def f(b: bool) -> int:
  return (1 if b else 1.0)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

