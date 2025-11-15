/***
 * Name: test_parser_tuples_lists
 * Purpose: Tuple/list corner cases: paren vs single-element tuple, trailing comma handling, nesting.
 */
#include <gtest/gtest.h>
#include <stdexcept>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "tpl.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserTuples, ParenVsSingleElementTuple) {
  const char* src =
      "def main() -> int:\n"
      "  a = (1)\n"
      "  b = (1,)\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 3u);
  const auto* asgA = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_TRUE(asgA->value);
  EXPECT_EQ(asgA->value->kind, ast::NodeKind::IntLiteral);
  const auto* asgB = static_cast<const ast::AssignStmt*>(fn.body[1].get());
  ASSERT_TRUE(asgB->value);
  EXPECT_EQ(asgB->value->kind, ast::NodeKind::TupleLiteral);
  const auto* tupB = static_cast<const ast::TupleLiteral*>(asgB->value.get());
  ASSERT_EQ(tupB->elements.size(), 1u);
}

TEST(ParserTuples, TrailingCommaAllowed) {
  const char* src =
      "def main() -> int:\n"
      "  t = (1, 2,)\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_TRUE(asg->value);
  ASSERT_EQ(asg->value->kind, ast::NodeKind::TupleLiteral);
  const auto* tup = static_cast<const ast::TupleLiteral*>(asg->value.get());
  ASSERT_EQ(tup->elements.size(), 2u);
}

TEST(ParserLists, EmptyAndMulti) {
  const char* src =
      "def main() -> int:\n"
      "  a = []\n"
      "  b = [1, 2, 3]\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 3u);
  const auto* asgA = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asgA->value->kind, ast::NodeKind::ListLiteral);
  const auto* listA = static_cast<const ast::ListLiteral*>(asgA->value.get());
  EXPECT_TRUE(listA->elements.empty());
  const auto* asgB = static_cast<const ast::AssignStmt*>(fn.body[1].get());
  ASSERT_EQ(asgB->value->kind, ast::NodeKind::ListLiteral);
  const auto* listB = static_cast<const ast::ListLiteral*>(asgB->value.get());
  ASSERT_EQ(listB->elements.size(), 3u);
}

TEST(ParserLists, TrailingCommaDisallowed) {
  const char* src =
      "def main() -> int:\n"
      "  a = [1, 2, ]\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "perr.py");
  parse::Parser P(L);
  EXPECT_THROW({ auto m = P.parseModule(); (void)m; }, std::runtime_error);
}

