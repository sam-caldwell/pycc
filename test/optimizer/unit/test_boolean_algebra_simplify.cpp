/***
 * Name: test_boolean_algebra_simplify
 * Purpose: Verify boolean algebra simplifications for and/or and double-not.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/AlgebraicSimplify.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrcBA(const char* src) {
  lex::Lexer L; L.pushString(src, "bool_alg.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(AlgebraicSimplify, BoolAndOrId) {
  const char* src =
      "def f(a: bool) -> bool:\n"
      "  return a and True\n"
      "def g(a: bool) -> bool:\n"
      "  return a or False\n"
      "def h(a: bool) -> bool:\n"
      "  return True and a\n"
      "def i(a: bool) -> bool:\n"
      "  return False or a\n"
      "def j(a: bool) -> bool:\n"
      "  return False and a\n"
      "def k(a: bool) -> bool:\n"
      "  return True or a\n";
  auto mod = parseSrcBA(src);
  opt::AlgebraicSimplify alg;
  const auto rewrites = alg.run(*mod);
  EXPECT_GE(rewrites, 4u);

  // f: return a
  {
    const auto& fn = *mod->functions[0];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::Name);
  }
  // g: return a
  {
    const auto& fn = *mod->functions[1];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::Name);
  }
  // h: return a
  {
    const auto& fn = *mod->functions[2];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::Name);
  }
  // i: return a
  {
    const auto& fn = *mod->functions[3];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::Name);
  }
  // j: return False
  {
    const auto& fn = *mod->functions[4];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::BoolLiteral);
    const auto* bl = static_cast<const ast::BoolLiteral*>(ret->value.get());
    EXPECT_FALSE(bl->value);
  }
  // k: return True
  {
    const auto& fn = *mod->functions[5];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::BoolLiteral);
    const auto* bl = static_cast<const ast::BoolLiteral*>(ret->value.get());
    EXPECT_TRUE(bl->value);
  }
}

TEST(AlgebraicSimplify, DoubleNot) {
  const char* src =
      "def n(a: bool) -> bool:\n"
      "  return not (not a)\n";
  auto mod = parseSrcBA(src);
  opt::AlgebraicSimplify alg;
  const auto rewrites = alg.run(*mod);
  EXPECT_GE(rewrites, 1u);
  const auto& fn = *mod->functions[0];
  const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
  ASSERT_EQ(ret->value->kind, ast::NodeKind::Name);
}
