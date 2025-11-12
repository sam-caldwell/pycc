/***
 * Name: test_parser
 * Purpose: Basic parser tests for assignments and function calls.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(Parser, AssignAndCall) {
  const char* src =
      "def add() -> int:\n"
      "  return 5\n"
      "def main() -> int:\n"
      "  x = add(2, 3)\n"
      "  return x\n";
  lex::Lexer L; L.pushString(src, "test.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  ASSERT_TRUE(mod);
  ASSERT_EQ(mod->functions.size(), 2u);

  const auto& mainFn = *mod->functions[1];
  ASSERT_EQ(mainFn.name, "main");
  ASSERT_EQ(mainFn.body.size(), 2u);

  // First stmt: assign x = add(2,3)
  const auto* s0 = mainFn.body[0].get();
  ASSERT_EQ(s0->kind, ast::NodeKind::AssignStmt);
  const auto* asg = static_cast<const ast::AssignStmt*>(s0);
  EXPECT_EQ(asg->target, "x");
  ASSERT_EQ(asg->value->kind, ast::NodeKind::Call);
  const auto* call = static_cast<const ast::Call*>(asg->value.get());
  ASSERT_EQ(call->callee->kind, ast::NodeKind::Name);
  const auto* name = static_cast<const ast::Name*>(call->callee.get());
  EXPECT_EQ(name->id, "add");
  ASSERT_EQ(call->args.size(), 2u);
  ASSERT_EQ(call->args[0]->kind, ast::NodeKind::IntLiteral);
  ASSERT_EQ(call->args[1]->kind, ast::NodeKind::IntLiteral);

  // Second stmt: return x
  const auto* s1 = mainFn.body[1].get();
  ASSERT_EQ(s1->kind, ast::NodeKind::ReturnStmt);
  const auto* ret = static_cast<const ast::ReturnStmt*>(s1);
  ASSERT_EQ(ret->value->kind, ast::NodeKind::Name);
  const auto* retName = static_cast<const ast::Name*>(ret->value.get());
  EXPECT_EQ(retName->id, "x");
}

TEST(Parser, FunctionParamsTyped) {
  const char* src =
      "def add(a: int, b: int) -> int:\n"
      "  return a\n";
  lex::Lexer L; L.pushString(src, "test.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  ASSERT_TRUE(mod);
  ASSERT_EQ(mod->functions.size(), 1u);
  const auto& addFn = *mod->functions[0];
  ASSERT_EQ(addFn.name, "add");
  ASSERT_EQ(addFn.params.size(), 2u);
  EXPECT_EQ(addFn.params[0].name, "a");
  EXPECT_EQ(addFn.params[1].name, "b");
}
