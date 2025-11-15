/***
 * Name: test_algebraic_identities
 * Purpose: Verify algebraic simplification for identities with zero/one.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/AlgebraicSimplify.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrcAlg(const char* src) {
  lex::Lexer L; L.pushString(src, "alg.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(AlgebraicSimplify, AddZeroAndMulOne) {
  const char* src =
      "def main() -> int:\n"
      "  x = 5\n"
      "  y = x + 0\n"
      "  z = 1 * x\n"
      "  return y + z\n";
  auto mod = parseSrcAlg(src);
  opt::AlgebraicSimplify alg;
  const auto rewrites = alg.run(*mod);
  EXPECT_GE(rewrites, 2u);
  const auto& fn = *mod->functions[0];
  // y = x; z = x after simplification; y+z remains but both sides names
  ASSERT_EQ(fn.body.size(), 4u);
  const auto* asgY = static_cast<const ast::AssignStmt*>(fn.body[1].get());
  ASSERT_TRUE(asgY->value);
  EXPECT_EQ(asgY->value->kind, ast::NodeKind::Name);
  const auto* asgZ = static_cast<const ast::AssignStmt*>(fn.body[2].get());
  ASSERT_TRUE(asgZ->value);
  EXPECT_EQ(asgZ->value->kind, ast::NodeKind::Name);
}

TEST(AlgebraicSimplify, MulZeroBecomesZero) {
  const char* src =
      "def main() -> int:\n"
      "  x = 7\n"
      "  return x * 0\n";
  auto mod = parseSrcAlg(src);
  opt::AlgebraicSimplify alg;
  const auto rewrites = alg.run(*mod);
  EXPECT_GE(rewrites, 1u);
  const auto& fn = *mod->functions[0];
  const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[2].get());
  ASSERT_TRUE(ret->value);
  EXPECT_EQ(ret->value->kind, ast::NodeKind::IntLiteral);
  const auto* lit0 = static_cast<const ast::IntLiteral*>(ret->value.get());
  EXPECT_EQ(lit0->value, 0);
}

