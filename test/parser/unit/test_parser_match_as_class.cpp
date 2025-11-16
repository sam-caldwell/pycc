/***
 * Name: test_parser_match_as_class
 * Purpose: Verify 'as' and simple class patterns.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserMatch, AsAndClassPattern) {
  const char* src =
      "def main() -> int:\n"
      "  match x:\n"
      "    case 1 as a:\n"
      "      return 1\n"
      "    case Point(x, y):\n"
      "      return 2\n";
  lex::Lexer L; L.pushString(src, "m2.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  ASSERT_TRUE(mod);
  const auto& fn = *mod->functions[0];
  const auto* ms = static_cast<const ast::MatchStmt*>(fn.body[0].get());
  ASSERT_EQ(ms->cases.size(), 2u);
  ASSERT_EQ(ms->cases[0]->pattern->kind, ast::NodeKind::PatternAs);
  const auto* pas = static_cast<const ast::PatternAs*>(ms->cases[0]->pattern.get());
  ASSERT_TRUE(pas->pattern);
  ASSERT_EQ(pas->name, std::string("a"));
  ASSERT_EQ(ms->cases[1]->pattern->kind, ast::NodeKind::PatternClass);
  const auto* pcl = static_cast<const ast::PatternClass*>(ms->cases[1]->pattern.get());
  ASSERT_EQ(pcl->className, std::string("Point"));
  ASSERT_EQ(pcl->args.size(), 2u);
  ASSERT_EQ(pcl->args[0]->kind, ast::NodeKind::PatternName);
  ASSERT_EQ(pcl->args[1]->kind, ast::NodeKind::PatternName);
}

