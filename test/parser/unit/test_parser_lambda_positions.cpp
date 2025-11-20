/***
 * Name: test_parser_lambda_positions
 * Purpose: Ensure AST nodes track line/col for diagnostics (names/returns).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserPositions, NamesAndReturnsCarryLocations) {
  const char* src =
      "def f(a: int) -> int:\n"
      "  x = a\n"
      "  return x\n";
  lex::Lexer L; L.pushString(src, "pos.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  const auto& fn = *mod->functions[0];
  // Check positions on Assign (value Name)
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_TRUE(asg->value);
  const auto* nm = static_cast<const ast::Name*>(asg->value.get());
  EXPECT_EQ(nm->file, std::string("pos.py"));
  EXPECT_EQ(nm->line, 2);
  EXPECT_GT(nm->col, 1);
  // Check Return value positions
  const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[1].get());
  ASSERT_TRUE(ret->value);
  const auto* nm2 = static_cast<const ast::Name*>(ret->value.get());
  EXPECT_EQ(nm2->line, 3);
}

