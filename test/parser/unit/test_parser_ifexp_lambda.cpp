/***
 * Name: test_parser_ifexp_lambda
 * Purpose: Verify parsing of if-expression and lambda expressions.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserIfExpLambda, IfExpression) {
  const char* src =
      "def main() -> int:\n"
      "  x = 1 if True else 2\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "ife.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  ASSERT_TRUE(mod);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::IfExpr);
}

TEST(ParserIfExpLambda, LambdaBasic) {
  const char* src =
      "def main() -> int:\n"
      "  f = lambda a, b: a\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "lam.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  ASSERT_TRUE(mod);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::LambdaExpr);
}

