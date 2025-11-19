/***
 * Name: test_constant_fold_calls_and_unary
 * Purpose: Drive ConstantFold to 100% (targeted subset): bitnot, len of literals, isinstance of literals, None compares.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/ConstantFold.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "cf_calls_unary.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ConstantFoldCallsUnary, UnaryBitNotOnInt) {
  const char* src =
      "def main() -> int:\n"
      "  return ~5\n";
  auto mod = parseSrc(src);
  opt::ConstantFold fold;
  const auto rewrites = fold.run(*mod);
  EXPECT_GE(rewrites, 1u);
  const auto& fn = *mod->functions[0];
  const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
  ASSERT_EQ(ret->value->kind, ast::NodeKind::IntLiteral);
  const auto* lit = static_cast<const ast::IntLiteral*>(ret->value.get());
  EXPECT_EQ(lit->value, ~5);
}

TEST(ConstantFoldCallsUnary, LenOfLiterals) {
  const char* src =
      "def a() -> int:\n"
      "  return len((1,2,3))\n"
      "def b() -> int:\n"
      "  return len([1,2])\n"
      "def c() -> int:\n"
      "  return len(\"abcd\")\n";
  auto mod = parseSrc(src);
  opt::ConstantFold fold;
  const auto rewrites = fold.run(*mod);
  EXPECT_GE(rewrites, 3u);
  {
    const auto& fn = *mod->functions[0];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::IntLiteral);
    const auto* lit = static_cast<const ast::IntLiteral*>(ret->value.get());
    EXPECT_EQ(lit->value, 3);
  }
  {
    const auto& fn = *mod->functions[1];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::IntLiteral);
    const auto* lit = static_cast<const ast::IntLiteral*>(ret->value.get());
    EXPECT_EQ(lit->value, 2);
  }
  {
    const auto& fn = *mod->functions[2];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::IntLiteral);
    const auto* lit = static_cast<const ast::IntLiteral*>(ret->value.get());
    EXPECT_EQ(lit->value, 4);
  }
}

TEST(ConstantFoldCallsUnary, IsInstanceOfLiterals) {
  const char* src =
      "def a() -> bool:\n"
      "  return isinstance(1, int)\n"
      "def b() -> bool:\n"
      "  return isinstance(1.0, int)\n"
      "def c() -> bool:\n"
      "  return isinstance(True, bool)\n";
  auto mod = parseSrc(src);
  opt::ConstantFold fold;
  const auto rewrites = fold.run(*mod);
  EXPECT_GE(rewrites, 3u);
  {
    const auto& fn = *mod->functions[0];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::BoolLiteral);
    const auto* bl = static_cast<const ast::BoolLiteral*>(ret->value.get());
    EXPECT_TRUE(bl->value);
  }
  {
    const auto& fn = *mod->functions[1];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::BoolLiteral);
    const auto* bl = static_cast<const ast::BoolLiteral*>(ret->value.get());
    EXPECT_FALSE(bl->value);
  }
  {
    const auto& fn = *mod->functions[2];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::BoolLiteral);
    const auto* bl = static_cast<const ast::BoolLiteral*>(ret->value.get());
    EXPECT_TRUE(bl->value);
  }
}

TEST(ConstantFoldCallsUnary, NoneEquality) {
  const char* src =
      "def a() -> bool:\n"
      "  return None == None\n"
      "def b() -> bool:\n"
      "  return None != None\n";
  auto mod = parseSrc(src);
  opt::ConstantFold fold;
  const auto rewrites = fold.run(*mod);
  EXPECT_GE(rewrites, 2u);
  {
    const auto& fn = *mod->functions[0];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::BoolLiteral);
    const auto* bl = static_cast<const ast::BoolLiteral*>(ret->value.get());
    EXPECT_TRUE(bl->value);
  }
  {
    const auto& fn = *mod->functions[1];
    const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
    ASSERT_EQ(ret->value->kind, ast::NodeKind::BoolLiteral);
    const auto* bl = static_cast<const ast::BoolLiteral*>(ret->value.get());
    EXPECT_FALSE(bl->value);
  }
}

