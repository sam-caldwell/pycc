/***
 * Name: test_parser_match_extended
 * Purpose: Extend pattern matching coverage: starred sequence, mapping rest, and class kwargs.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/Pattern.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "match_ext.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserMatchExtended, SequenceStarred) {
  const char* src =
      "def main() -> int:\n"
      "  match x:\n"
      "    case [a, *rest, b]:\n"
      "      pass\n"
      "    case (a, *_, b):\n"
      "      pass\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* ms = static_cast<const ast::MatchStmt*>(fn.body[0].get());
  ASSERT_EQ(ms->cases.size(), 2u);
  const auto* ps1 = static_cast<const ast::PatternSequence*>(ms->cases[0]->pattern.get());
  ASSERT_EQ(ps1->elements.size(), 3u);
  ASSERT_EQ(ps1->elements[1]->kind, ast::NodeKind::PatternStar);
  const auto* ps2 = static_cast<const ast::PatternSequence*>(ms->cases[1]->pattern.get());
  ASSERT_EQ(ps2->elements.size(), 3u);
  ASSERT_EQ(ps2->elements[1]->kind, ast::NodeKind::PatternStar);
}

TEST(ParserMatchExtended, MappingRest) {
  const char* src =
      "def main() -> int:\n"
      "  match x:\n"
      "    case {**rest, 'k': v}:\n"
      "      pass\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* ms = static_cast<const ast::MatchStmt*>(fn.body[0].get());
  ASSERT_EQ(ms->cases.size(), 1u);
  ASSERT_EQ(ms->cases[0]->pattern->kind, ast::NodeKind::PatternMapping);
  const auto* pm = static_cast<const ast::PatternMapping*>(ms->cases[0]->pattern.get());
  ASSERT_TRUE(pm->hasRest);
  ASSERT_EQ(pm->restName, std::string("rest"));
  ASSERT_EQ(pm->items.size(), 1u);
}

TEST(ParserMatchExtended, ClassKwArgs) {
  const char* src =
      "def main() -> int:\n"
      "  match x:\n"
      "    case Point(x=a, y=b):\n"
      "      pass\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* ms = static_cast<const ast::MatchStmt*>(fn.body[0].get());
  ASSERT_EQ(ms->cases.size(), 1u);
  ASSERT_EQ(ms->cases[0]->pattern->kind, ast::NodeKind::PatternClass);
  const auto* pc = static_cast<const ast::PatternClass*>(ms->cases[0]->pattern.get());
  ASSERT_TRUE(pc->args.empty());
  ASSERT_EQ(pc->kwargs.size(), 2u);
  EXPECT_EQ(pc->kwargs[0].first, std::string("x"));
  EXPECT_EQ(pc->kwargs[1].first, std::string("y"));
}

