/***
 * Name: test_constant_fold
 * Purpose: Verify constant folding for ints, floats, comparisons, and unary negation.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/ConstantFold.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrcCF(const char* src) {
  lex::Lexer L; L.pushString(src, "cf.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ConstantFold, IntArithmeticAndCmp) {
  const char* src =
      "def main() -> int:\n"
      "  return (2 + 3) * (10 - 5)\n";
  auto mod = parseSrcCF(src);
  opt::ConstantFold fold;
  const auto rewrites = fold.run(*mod);
  EXPECT_GE(rewrites, 2u);
  ASSERT_EQ(mod->functions.size(), 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 1u);
  const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
  ASSERT_TRUE(ret->value);
  ASSERT_EQ(ret->value->kind, ast::NodeKind::IntLiteral);
  const auto* lit = static_cast<const ast::IntLiteral*>(ret->value.get());
  EXPECT_EQ(lit->value, 25);
}

TEST(ConstantFold, FloatArithmeticAndUnary) {
  const char* src =
      "def main() -> float:\n"
      "  return -(1.5 + 2.5)\n";
  auto mod = parseSrcCF(src);
  opt::ConstantFold fold;
  const auto rewrites = fold.run(*mod);
  EXPECT_GE(rewrites, 2u);
  const auto& fn = *mod->functions[0];
  const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
  ASSERT_TRUE(ret->value);
  ASSERT_EQ(ret->value->kind, ast::NodeKind::FloatLiteral);
  const auto* fl = static_cast<const ast::FloatLiteral*>(ret->value.get());
  EXPECT_DOUBLE_EQ(fl->value, -4.0);
}

TEST(ConstantFold, IntComparisonsFoldToBool) {
  const char* src =
      "def f() -> bool:\n"
      "  return 3 < 4\n"
      "def g() -> bool:\n"
      "  return 5 == 5\n";
  auto mod = parseSrcCF(src);
  opt::ConstantFold fold;
  const auto rewrites = fold.run(*mod);
  EXPECT_GE(rewrites, 2u);
  const auto& f = *mod->functions[0];
  const auto* r1 = static_cast<const ast::ReturnStmt*>(f.body[0].get());
  ASSERT_EQ(r1->value->kind, ast::NodeKind::BoolLiteral);
  const auto* b1 = static_cast<const ast::BoolLiteral*>(r1->value.get());
  EXPECT_TRUE(b1->value);
  const auto& g = *mod->functions[1];
  const auto* r2 = static_cast<const ast::ReturnStmt*>(g.body[0].get());
  ASSERT_EQ(r2->value->kind, ast::NodeKind::BoolLiteral);
  const auto* b2 = static_cast<const ast::BoolLiteral*>(r2->value.get());
  EXPECT_TRUE(b2->value);
}

