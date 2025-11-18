/***
 * Name: test_bitwise_shift_simplify
 * Purpose: Verify simplifications for bitwise ops and shifts.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/AlgebraicSimplify.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrcBS(const char* src) {
  lex::Lexer L; L.pushString(src, "bitshift.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(AlgebraicSimplify, BitwiseIdentities) {
  const char* src =
      "def f(x: int) -> int:\n"
      "  return x | 0\n"
      "def g(x: int) -> int:\n"
      "  return x & 0\n"
      "def h(x: int) -> int:\n"
      "  return x ^ 0\n"
      "def i(x: int) -> int:\n"
      "  return x & -1\n"
      "def j(x: int) -> int:\n"
      "  return x ^ x\n"
      "def k(x: int) -> int:\n"
      "  return x | x\n";
  auto mod = parseSrcBS(src);
  opt::AlgebraicSimplify alg;
  const auto rewrites = alg.run(*mod);
  EXPECT_GE(rewrites, 5u);

  // f: x | 0 -> x
  {
    const auto& fn = *mod->functions[0];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::Name);
  }
  // g: x & 0 -> 0
  {
    const auto& fn = *mod->functions[1];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::IntLiteral);
    const auto* lit0 = static_cast<const ast::IntLiteral*>(ret->value.get());
    EXPECT_EQ(lit0->value, 0);
  }
  // h: x ^ 0 -> x
  {
    const auto& fn = *mod->functions[2];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::Name);
  }
  // i: x & -1 -> x
  {
    const auto& fn = *mod->functions[3];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::Name);
  }
  // j: x ^ x -> 0
  {
    const auto& fn = *mod->functions[4];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::IntLiteral);
    const auto* lit0 = static_cast<const ast::IntLiteral*>(ret->value.get());
    EXPECT_EQ(lit0->value, 0);
  }
  // k: x | x -> x
  {
    const auto& fn = *mod->functions[5];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::Name);
  }
}

TEST(AlgebraicSimplify, ShiftByZero) {
  const char* src =
      "def s(x: int) -> int:\n"
      "  return x << 0\n"
      "def t(x: int) -> int:\n"
      "  return x >> 0\n";
  auto mod = parseSrcBS(src);
  opt::AlgebraicSimplify alg;
  const auto rewrites = alg.run(*mod);
  EXPECT_GE(rewrites, 2u);

  const auto& s = *mod->functions[0];
  const auto* r1 = static_cast<const ast::ReturnStmt*>(s.body[0].get());
  ASSERT_EQ(r1->value->kind, ast::NodeKind::Name);

  const auto& t = *mod->functions[1];
  const auto* r2 = static_cast<const ast::ReturnStmt*>(t.body[0].get());
  ASSERT_EQ(r2->value->kind, ast::NodeKind::Name);
}

