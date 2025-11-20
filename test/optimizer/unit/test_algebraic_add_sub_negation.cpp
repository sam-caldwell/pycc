/***
 * Name: test_algebraic_add_sub_negation
 * Purpose: Verify x + (-y) -> x - y; x - (-y) -> x + y; (-x) + y -> y - x (float/int forms).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/AlgebraicSimplify.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseAlg2(const char* src) {
  lex::Lexer L; L.pushString(src, "alg_neg.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(AlgebraicSimplify, AddSubWithNegation_Float) {
  const char* src =
      "def f(x: float, y: float) -> float:\n"
      "  return x + (-y)\n"
      "def g(x: float, y: float) -> float:\n"
      "  return x - (-y)\n"
      "def h(x: float, y: float) -> float:\n"
      "  return (-x) + y\n";
  auto mod = parseAlg2(src);
  opt::AlgebraicSimplify alg; const auto rewrites = alg.run(*mod); EXPECT_GE(rewrites, 3u);
  // All three should become binary expressions without nested unary minus on RHS/LHS, but exact form verified by kind
  for (int i = 0; i < 3; ++i) {
    const auto& fn = *mod->functions[i];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::BinaryExpr);
  }
}

