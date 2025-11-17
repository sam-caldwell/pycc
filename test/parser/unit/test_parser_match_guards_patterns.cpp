/***
 * Name: test_parser_match_guards_patterns
 * Purpose: Verify match guards and broader patterns (sequence/mapping).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserMatch, GuardsAndPatterns) {
  const char* src =
      "def main() -> int:\n"
      "  match x:\n"
      "    case [a, b]:\n"
      "      pass\n"
      "    case {\"k\": v} if True:\n"
      "      pass\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "m.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  const auto& fn = *mod->functions[0];
  const auto* ms = static_cast<const ast::MatchStmt*>(fn.body[0].get());
  ASSERT_EQ(ms->cases.size(), 2u);
  EXPECT_EQ(ms->cases[0]->pattern->kind, ast::NodeKind::PatternSequence);
  EXPECT_EQ(ms->cases[1]->pattern->kind, ast::NodeKind::PatternMapping);
  EXPECT_TRUE(ms->cases[1]->guard != nullptr);
}

