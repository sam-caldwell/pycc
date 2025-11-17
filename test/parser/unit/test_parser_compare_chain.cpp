/***
 * Name: test_parser_compare_chain
 * Purpose: Verify parsing of chained comparisons builds Compare AST.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserCompare, Chain) {
  const char* src =
      "def main() -> int:\n"
      "  y = 1 < 2 < 3\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "cmp.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::Compare);
}

