/***
 * Name: test_parser_match_basic
 * Purpose: Verify match/case parses with literal, name, wildcard, OR patterns.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserMatch, BasicPatterns) {
  const char* src =
      "def main() -> int:\n"
      "  match x:\n"
      "    case 1:\n"
      "      return 1\n"
      "    case a | b:\n"
      "      return 2\n"
      "    case _:\n"
      "      return 0\n";
  lex::Lexer L; L.pushString(src, "m.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  ASSERT_TRUE(mod);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::MatchStmt);
  const auto* ms = static_cast<const ast::MatchStmt*>(fn.body[0].get());
  ASSERT_EQ(ms->cases.size(), 3u);
  // case 1
  ASSERT_EQ(ms->cases[0]->pattern->kind, ast::NodeKind::PatternLiteral);
  // case a | b
  ASSERT_EQ(ms->cases[1]->pattern->kind, ast::NodeKind::PatternOr);
  const auto* por = static_cast<const ast::PatternOr*>(ms->cases[1]->pattern.get());
  ASSERT_EQ(por->patterns.size(), 2u);
  ASSERT_EQ(por->patterns[0]->kind, ast::NodeKind::PatternName);
  ASSERT_EQ(por->patterns[1]->kind, ast::NodeKind::PatternName);
  // case _
  ASSERT_EQ(ms->cases[2]->pattern->kind, ast::NodeKind::PatternWildcard);
}

