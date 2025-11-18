/***
 * Name: test_pow_simplify
 * Purpose: Verify exponentiation identity simplification (x ** 1 -> x).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/AlgebraicSimplify.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrcPow(const char* src) {
  lex::Lexer L; L.pushString(src, "pow.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(AlgebraicSimplify, PowByOne) {
  const char* src =
      "def p(x: int) -> int:\n"
      "  return x ** 1\n"
      "def q(x: float) -> float:\n"
      "  return x ** 1\n";
  auto mod = parseSrcPow(src);
  opt::AlgebraicSimplify alg;
  const auto rewrites = alg.run(*mod);
  EXPECT_GE(rewrites, 2u);

  const auto& p = *mod->functions[0];
  const auto* r1 = static_cast<const ast::ReturnStmt*>(p.body[0].get());
  ASSERT_EQ(r1->value->kind, ast::NodeKind::Name);

  const auto& q = *mod->functions[1];
  const auto* r2 = static_cast<const ast::ReturnStmt*>(q.body[0].get());
  ASSERT_EQ(r2->value->kind, ast::NodeKind::Name);
}

