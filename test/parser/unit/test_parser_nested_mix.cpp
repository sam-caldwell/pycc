/***
 * Name: test_parser_nested_mix
 * Purpose: Nested tuple/list mixtures parse correctly.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserNested, TupleListMixture) {
  const char* src =
      "def main() -> int:\n"
      "  x = (1, [2, (3, 4)], 5)\n"
      "  y = [ (6, 7), [8, 9] ]\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "mix.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  ASSERT_TRUE(mod);
  ASSERT_EQ(mod->functions.size(), 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 3u);
  // x assignment: tuple with 3 elements; second is list having Int and Tuple
  const auto* asgX = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asgX->value->kind, ast::NodeKind::TupleLiteral);
  const auto* tupX = static_cast<const ast::TupleLiteral*>(asgX->value.get());
  ASSERT_EQ(tupX->elements.size(), 3u);
  ASSERT_EQ(tupX->elements[0]->kind, ast::NodeKind::IntLiteral);
  ASSERT_EQ(tupX->elements[1]->kind, ast::NodeKind::ListLiteral);
  const auto* innerList = static_cast<const ast::ListLiteral*>(tupX->elements[1].get());
  ASSERT_EQ(innerList->elements.size(), 2u);
  ASSERT_EQ(innerList->elements[0]->kind, ast::NodeKind::IntLiteral);
  ASSERT_EQ(innerList->elements[1]->kind, ast::NodeKind::TupleLiteral);
  // y assignment: list containing Tuple and List
  const auto* asgY = static_cast<const ast::AssignStmt*>(fn.body[1].get());
  ASSERT_EQ(asgY->value->kind, ast::NodeKind::ListLiteral);
  const auto* listY = static_cast<const ast::ListLiteral*>(asgY->value.get());
  ASSERT_EQ(listY->elements.size(), 2u);
  ASSERT_EQ(listY->elements[0]->kind, ast::NodeKind::TupleLiteral);
  ASSERT_EQ(listY->elements[1]->kind, ast::NodeKind::ListLiteral);
}

