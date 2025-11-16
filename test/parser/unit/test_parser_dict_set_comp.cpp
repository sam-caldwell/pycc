/***
 * Name: test_parser_dict_set_comp
 * Purpose: Verify parsing of dict/set literals and comprehensions.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "dsc.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserDictSet, LiteralsAndEmpty) {
  const char* src =
      "def main() -> int:\n"
      "  a = {1}\n"
      "  b = {1: 2, 3: 4}\n"
      "  c = {}\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asgA = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asgA->value->kind, ast::NodeKind::SetLiteral);
  const auto* asgB = static_cast<const ast::AssignStmt*>(fn.body[1].get());
  ASSERT_EQ(asgB->value->kind, ast::NodeKind::DictLiteral);
  const auto* asgC = static_cast<const ast::AssignStmt*>(fn.body[2].get());
  ASSERT_EQ(asgC->value->kind, ast::NodeKind::DictLiteral);
}

TEST(ParserDictSet, Comprehensions) {
  const char* src =
      "def main() -> int:\n"
      "  a = [x for x in [1,2] if x]\n"
      "  b = {x for x in [1,2]}\n"
      "  c = {x: x*x for x in [1,2]}\n"
      "  d = (x for x in [1])\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(static_cast<const ast::AssignStmt*>(fn.body[0].get())->value->kind, ast::NodeKind::ListComp);
  ASSERT_EQ(static_cast<const ast::AssignStmt*>(fn.body[1].get())->value->kind, ast::NodeKind::SetComp);
  ASSERT_EQ(static_cast<const ast::AssignStmt*>(fn.body[2].get())->value->kind, ast::NodeKind::DictComp);
  ASSERT_EQ(static_cast<const ast::AssignStmt*>(fn.body[3].get())->value->kind, ast::NodeKind::GeneratorExpr);
}

