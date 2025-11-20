/***
 * Name: test_algebraic_pow_zero_floordiv_mod_id
 * Purpose: Verify identities: x ** 0 -> 1; x // 1 -> x; x % 1 -> 0; 0 % x -> 0; and 0 << x / 0 >> x -> 0.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/AlgebraicSimplify.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseAlg(const char* src) {
  lex::Lexer L; L.pushString(src, "alg_ids.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(AlgebraicSimplify, PowZeroAndFloorDivModInt) {
  const char* src =
      "def p(x: int) -> int:\n"
      "  return x ** 0\n"
      "def q(x: int) -> int:\n"
      "  return x // 1\n"
      "def r(x: int) -> int:\n"
      "  return x % 1\n"
      "def s(x: int) -> int:\n"
      "  return 0 % x\n";
  auto mod = parseAlg(src);
  opt::AlgebraicSimplify alg; const auto rewrites = alg.run(*mod); EXPECT_GE(rewrites, 4u);
  // p: 1
  {
    const auto& fn = *mod->functions[0]; const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::IntLiteral);
  }
  // q: x
  {
    const auto& fn = *mod->functions[1]; const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::Name);
  }
  // r: 0
  {
    const auto& fn = *mod->functions[2]; const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::IntLiteral);
    const auto* lit0 = static_cast<const ast::IntLiteral*>(ret->value.get()); EXPECT_EQ(lit0->value, 0);
  }
  // s: 0
  {
    const auto& fn = *mod->functions[3]; const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::IntLiteral);
    const auto* lit0 = static_cast<const ast::IntLiteral*>(ret->value.get()); EXPECT_EQ(lit0->value, 0);
  }
}

TEST(AlgebraicSimplify, ShiftZeroLhs) {
  const char* src =
      "def a(x: int) -> int:\n"
      "  return 0 << x\n"
      "def b(x: int) -> int:\n"
      "  return 0 >> x\n";
  auto mod = parseAlg(src);
  opt::AlgebraicSimplify alg; const auto rewrites = alg.run(*mod); EXPECT_GE(rewrites, 2u);
  const auto& a = *mod->functions[0]; const auto* r1 = static_cast<const ast::ReturnStmt*>(a.body[0].get());
  ASSERT_EQ(r1->value->kind, ast::NodeKind::IntLiteral);
  const auto& b = *mod->functions[1]; const auto* r2 = static_cast<const ast::ReturnStmt*>(b.body[0].get());
  ASSERT_EQ(r2->value->kind, ast::NodeKind::IntLiteral);
}

