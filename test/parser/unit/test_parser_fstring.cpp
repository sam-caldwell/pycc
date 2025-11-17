/***
 * Name: test_parser_fstring
 * Purpose: Verify f-strings tokenize and parse as FStringLiteral.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserFString, Basic) {
  const char* src =
      "def main() -> int:\n"
      "  s = f\"hello {name}\"\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "fs.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::FStringLiteral);
}

