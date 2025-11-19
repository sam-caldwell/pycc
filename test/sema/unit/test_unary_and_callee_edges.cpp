/***
 * Name: test_unary_and_callee_edges
 * Purpose: Cover unary negation type error and unsupported callee expressions.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "unary_callee_edges.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(UnaryAndCalleeEdges, UnaryNegWrongTypeFails) {
  const char* src = R"PY(
def f() -> int:
  return -'a'
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(UnaryAndCalleeEdges, UnsupportedCalleeExpression) {
  const char* src = R"PY(
def g() -> int:
  return (1)(2)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

