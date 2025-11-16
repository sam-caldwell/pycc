/***
 * Name: test_parser_assign_destructuring
 * Purpose: Verify destructuring assignment sets Store ctx and fills targets.
 */
#include <gtest/gtest.h>
#include <stdexcept>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "assign.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserAssignDestructuring, TupleNamesStoreCtx) {
  const char* src =
      "def main() -> int:\n"
      "  a, b = 1, 2\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  ASSERT_TRUE(mod);
  ASSERT_EQ(mod->functions.size(), 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 2u);
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->targets.size(), 2u);
  ASSERT_EQ(asg->targets[0]->kind, ast::NodeKind::Name);
  ASSERT_EQ(asg->targets[1]->kind, ast::NodeKind::Name);
  const auto* a = static_cast<const ast::Name*>(asg->targets[0].get());
  const auto* b = static_cast<const ast::Name*>(asg->targets[1].get());
  EXPECT_EQ(a->ctx, ast::ExprContext::Store);
  EXPECT_EQ(b->ctx, ast::ExprContext::Store);
}

