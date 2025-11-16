/***
 * Name: test_parser_namedexpr
 * Purpose: Verify named expression parses into NamedExpr and nests inside parens/expressions.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserNamedExpr, BasicRHS) {
  const char* src =
      "def main() -> int:\n"
      "  x = (y := 3)\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "ne.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  ASSERT_TRUE(mod);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::NamedExpr);
  const auto* ne = static_cast<const ast::NamedExpr*>(asg->value.get());
  ASSERT_EQ(ne->target, std::string("y"));
}

