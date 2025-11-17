/***
 * Name: test_parser_dict_unpack
 * Purpose: Verify dict literal unpack entries and key:value pairs.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include <cstdio>
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserDict, UnpackAndPairs) {
  const char* src =
      "def main() -> int:\n"
      "  d = {'k': v, **a, **b}\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "du.py");
  // debug tokens using separate lexer to avoid interfering with Parser
  lex::Lexer L2; L2.pushString(src, "du.py");
  auto toks = L2.tokens();
  for (const auto& t : toks) { std::fprintf(stderr, "tok %s '%s'\n", pycc::lex::to_string(t.kind), t.text.c_str()); }
  parse::Parser P(L);
  auto mod = P.parseModule();
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::DictLiteral);
  const auto* dict = static_cast<const ast::DictLiteral*>(asg->value.get());
  EXPECT_EQ(dict->unpacks.size(), 2u);
  EXPECT_EQ(dict->items.size(), 1u);
}

TEST(ParserDict, UnpackOnly) {
  const char* src =
      "def main() -> int:\n"
      "  d = {**a}\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "du2.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::DictLiteral);
  const auto* dict = static_cast<const ast::DictLiteral*>(asg->value.get());
  EXPECT_EQ(dict->unpacks.size(), 1u);
  EXPECT_TRUE(dict->items.empty());
}

TEST(ParserDict, MixedOrders) {
  // {**a, 'k': v, **b}
  {
    const char* src =
        "def main() -> int:\n"
        "  d = {**a, 'k': v, **b}\n"
        "  return 0\n";
    lex::Lexer L; L.pushString(src, "du3.py");
    parse::Parser P(L);
    auto mod = P.parseModule();
    const auto& fn = *mod->functions[0];
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
    ASSERT_EQ(asg->value->kind, ast::NodeKind::DictLiteral);
    const auto* dict = static_cast<const ast::DictLiteral*>(asg->value.get());
    EXPECT_EQ(dict->unpacks.size(), 2u);
    EXPECT_EQ(dict->items.size(), 1u);
  }
  // {'k': v, **a, **b}
  {
    const char* src =
        "def main() -> int:\n"
        "  d = {'k': v, **a, **b}\n"
        "  return 0\n";
    lex::Lexer L; L.pushString(src, "du4.py");
    parse::Parser P(L);
    auto mod = P.parseModule();
    const auto& fn = *mod->functions[0];
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
    ASSERT_EQ(asg->value->kind, ast::NodeKind::DictLiteral);
    const auto* dict = static_cast<const ast::DictLiteral*>(asg->value.get());
    EXPECT_EQ(dict->unpacks.size(), 2u);
    EXPECT_EQ(dict->items.size(), 1u);
  }
}
