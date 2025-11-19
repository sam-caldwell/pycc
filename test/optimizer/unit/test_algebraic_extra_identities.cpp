/***
 * Name: test_algebraic_extra_identities
 * Purpose: Additional algebraic identities to push coverage to 100% targeted.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/AlgebraicSimplify.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "alg_extra.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(AlgebraicSimplifyExtra, ZeroMinusXBecomesNegX_Int) {
  const char* src =
      "def f(x: int) -> int:\n"
      "  return 0 - x\n";
  auto mod = parseSrc(src);
  opt::AlgebraicSimplify alg;
  const auto rewrites = alg.run(*mod);
  EXPECT_GE(rewrites, 1u);
  const auto& fn = *mod->functions[0];
  const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
  ASSERT_EQ(ret->value->kind, ast::NodeKind::UnaryExpr);
}

TEST(AlgebraicSimplifyExtra, ZeroMinusXBecomesNegX_Float) {
  const char* src =
      "def f(x: float) -> float:\n"
      "  return 0.0 - x\n";
  auto mod = parseSrc(src);
  opt::AlgebraicSimplify alg;
  const auto rewrites = alg.run(*mod);
  EXPECT_GE(rewrites, 1u);
  const auto& fn = *mod->functions[0];
  const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
  ASSERT_EQ(ret->value->kind, ast::NodeKind::UnaryExpr);
}

TEST(AlgebraicSimplifyExtra, FloatIdentities) {
  const char* src =
      "def a(x: float) -> float:\n"
      "  return x + 0.0\n"
      "def b(x: float) -> float:\n"
      "  return 1.0 * x\n"
      "def c(x: float) -> float:\n"
      "  return x * 0.0\n"
      "def d(x: float) -> float:\n"
      "  return x / 1.0\n";
  auto mod = parseSrc(src);
  opt::AlgebraicSimplify alg;
  const auto rewrites = alg.run(*mod);
  EXPECT_GE(rewrites, 4u);
  // a: return x
  {
    const auto& fn = *mod->functions[0];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::Name);
  }
  // b: return x
  {
    const auto& fn = *mod->functions[1];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::Name);
  }
  // c: return 0.0
  {
    const auto& fn = *mod->functions[2];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::FloatLiteral);
    const auto* fl = static_cast<const ast::FloatLiteral*>(ret->value.get());
    EXPECT_DOUBLE_EQ(fl->value, 0.0);
  }
  // d: return x
  {
    const auto& fn = *mod->functions[3];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::Name);
  }
}

TEST(AlgebraicSimplifyExtra, MulDivByNegOne_Int) {
  const char* src =
      "def a(x: int) -> int:\n"
      "  return -1 * x\n"
      "def b(x: int) -> int:\n"
      "  return x * -1\n"
      "def c(x: int) -> int:\n"
      "  return x / -1\n";
  auto mod = parseSrc(src);
  opt::AlgebraicSimplify alg; const auto rewrites = alg.run(*mod);
  EXPECT_GE(rewrites, 3u);
  for (int i = 0; i < 3; ++i) {
    const auto& fn = *mod->functions[i];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::UnaryExpr);
  }
}

TEST(AlgebraicSimplifyExtra, MulDivByNegOne_Float) {
  const char* src =
      "def a(x: float) -> float:\n"
      "  return -1.0 * x\n"
      "def b(x: float) -> float:\n"
      "  return x * -1.0\n"
      "def c(x: float) -> float:\n"
      "  return x / -1.0\n";
  auto mod = parseSrc(src);
  opt::AlgebraicSimplify alg; const auto rewrites = alg.run(*mod);
  EXPECT_GE(rewrites, 3u);
  for (int i = 0; i < 3; ++i) {
    const auto& fn = *mod->functions[i];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::UnaryExpr);
  }
}

TEST(AlgebraicSimplifyExtra, BitwiseOrWithNegOne) {
  const char* src =
      "def f(x: int) -> int:\n"
      "  return x | -1\n"
      "def g(x: int) -> int:\n"
      "  return -1 | x\n";
  auto mod = parseSrc(src);
  opt::AlgebraicSimplify alg; const auto rewrites = alg.run(*mod);
  EXPECT_GE(rewrites, 2u);
  for (int i = 0; i < 2; ++i) {
    const auto& fn = *mod->functions[i];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::IntLiteral);
    const auto* lit = static_cast<const ast::IntLiteral*>(ret->value.get());
    EXPECT_EQ(lit->value, -1);
  }
}

TEST(AlgebraicSimplifyExtra, BitwiseXorWithNegOne) {
  const char* src =
      "def f(x: int) -> int:\n"
      "  return x ^ -1\n"
      "def g(x: int) -> int:\n"
      "  return -1 ^ x\n";
  auto mod = parseSrc(src);
  opt::AlgebraicSimplify alg; const auto rewrites = alg.run(*mod);
  EXPECT_GE(rewrites, 2u);
  for (int i = 0; i < 2; ++i) {
    const auto& fn = *mod->functions[i];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::UnaryExpr);
  }
}

TEST(AlgebraicSimplifyExtra, PowOneToX) {
  const char* src =
      "def a(x: int) -> int:\n"
      "  return 1 ** x\n"
      "def b(x: float) -> float:\n"
      "  return 1.0 ** x\n";
  auto mod = parseSrc(src);
  opt::AlgebraicSimplify alg; const auto rewrites = alg.run(*mod);
  EXPECT_GE(rewrites, 2u);
  {
    const auto& fn = *mod->functions[0];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::IntLiteral);
    const auto* lit = static_cast<const ast::IntLiteral*>(ret->value.get());
    EXPECT_EQ(lit->value, 1);
  }
  {
    const auto& fn = *mod->functions[1];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::FloatLiteral);
    const auto* fl = static_cast<const ast::FloatLiteral*>(ret->value.get());
    EXPECT_DOUBLE_EQ(fl->value, 1.0);
  }
}

