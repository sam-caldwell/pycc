/***
 * Name: test_parser_arithmetic_ops
 * Purpose: Ensure all arithmetic and bitwise operators parse into the correct AST shapes.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/Binary.h"
#include "ast/Unary.h"
#include "ast/IntLiteral.h"
#include "ast/Name.h"
#include "ast/BinaryOperator.h"
#include "ast/UnaryOperator.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "arith.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserArithmetic, AddSub) {
  const char* src =
      "def f() -> int:\n"
      "  a = 1 + 2\n"
      "  b = 3 - 2\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  // a = 1 + 2
  {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
    ASSERT_EQ(asg->value->kind, ast::NodeKind::BinaryExpr);
    const auto* bin = static_cast<const ast::Binary*>(asg->value.get());
    EXPECT_EQ(bin->op, ast::BinaryOperator::Add);
    ASSERT_EQ(bin->lhs->kind, ast::NodeKind::IntLiteral);
    ASSERT_EQ(bin->rhs->kind, ast::NodeKind::IntLiteral);
  }
  // b = 3 - 2
  {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[1].get());
    ASSERT_EQ(asg->value->kind, ast::NodeKind::BinaryExpr);
    const auto* bin = static_cast<const ast::Binary*>(asg->value.get());
    EXPECT_EQ(bin->op, ast::BinaryOperator::Sub);
  }
}

TEST(ParserArithmetic, MulDivModFloorPow) {
  const char* src =
      "def f() -> int:\n"
      "  c = 2 * 3\n"
      "  d = 5 / 2\n"
      "  e = 5 % 2\n"
      "  f = 7 // 3\n"
      "  g = 2 ** 3\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  auto check = [&](int idx, ast::BinaryOperator op) {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[idx].get());
    ASSERT_EQ(asg->value->kind, ast::NodeKind::BinaryExpr);
    const auto* bin = static_cast<const ast::Binary*>(asg->value.get());
    EXPECT_EQ(bin->op, op);
  };
  check(0, ast::BinaryOperator::Mul);
  check(1, ast::BinaryOperator::Div);
  check(2, ast::BinaryOperator::Mod);
  check(3, ast::BinaryOperator::FloorDiv);
  check(4, ast::BinaryOperator::Pow);
}

TEST(ParserArithmetic, ShiftsAndBitwise) {
  const char* src =
      "def f() -> int:\n"
      "  h = x << y\n"
      "  i = x >> y\n"
      "  j = x & y\n"
      "  k = x | y\n"
      "  l = x ^ y\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  auto check = [&](int idx, ast::BinaryOperator op) {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[idx].get());
    ASSERT_EQ(asg->value->kind, ast::NodeKind::BinaryExpr);
    const auto* bin = static_cast<const ast::Binary*>(asg->value.get());
    EXPECT_EQ(bin->op, op);
  };
  check(0, ast::BinaryOperator::LShift);
  check(1, ast::BinaryOperator::RShift);
  check(2, ast::BinaryOperator::BitAnd);
  check(3, ast::BinaryOperator::BitOr);
  check(4, ast::BinaryOperator::BitXor);
}

TEST(ParserArithmetic, UnaryAndPrecedence) {
  const char* src =
      "def f() -> int:\n"
      "  m = -x\n"
      "  n = ~x\n"
      "  o = +x\n"
      "  p = -1\n"
      "  q = 2 + 3 * 4\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  // m = -x
  {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
    ASSERT_EQ(asg->value->kind, ast::NodeKind::UnaryExpr);
    const auto* un = static_cast<const ast::Unary*>(asg->value.get());
    EXPECT_EQ(un->op, ast::UnaryOperator::Neg);
  }
  // n = ~x
  {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[1].get());
    ASSERT_EQ(asg->value->kind, ast::NodeKind::UnaryExpr);
    const auto* un = static_cast<const ast::Unary*>(asg->value.get());
    EXPECT_EQ(un->op, ast::UnaryOperator::BitNot);
  }
  // o = +x  (unary plus is no-op; not a UnaryExpr in this parser)
  {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[2].get());
    ASSERT_EQ(asg->value->kind, ast::NodeKind::Name);
  }
  // p = -1  (folds into IntLiteral -1)
  {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[3].get());
    ASSERT_EQ(asg->value->kind, ast::NodeKind::IntLiteral);
    const auto* lit = static_cast<const ast::IntLiteral*>(asg->value.get());
    EXPECT_LT(lit->value, 0);
  }
  // q = 2 + 3 * 4 (precedence check)
  {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[4].get());
    ASSERT_EQ(asg->value->kind, ast::NodeKind::BinaryExpr);
    const auto* add = static_cast<const ast::Binary*>(asg->value.get());
    EXPECT_EQ(add->op, ast::BinaryOperator::Add);
    ASSERT_EQ(add->rhs->kind, ast::NodeKind::BinaryExpr);
    const auto* mul = static_cast<const ast::Binary*>(add->rhs.get());
    EXPECT_EQ(mul->op, ast::BinaryOperator::Mul);
  }
}

