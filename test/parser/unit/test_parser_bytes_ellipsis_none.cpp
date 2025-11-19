/***
 * Name: test_parser_bytes_ellipsis_none
 * Purpose: Increase parsePrimary coverage: bytes literal, ellipsis, and None literal as expression.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/NoneLiteral.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "ben.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserPrimary, BytesAndEllipsisAndNoneLiterals) {
  const char* src =
      "def main() -> int:\n"
      "  a = b'xyz'\n"
      "  b = ...\n"
      "  c = None\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asgA = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  EXPECT_EQ(asgA->value->kind, ast::NodeKind::BytesLiteral);
  const auto* asgB = static_cast<const ast::AssignStmt*>(fn.body[1].get());
  EXPECT_EQ(asgB->value->kind, ast::NodeKind::EllipsisLiteral);
  const auto* asgC = static_cast<const ast::AssignStmt*>(fn.body[2].get());
  EXPECT_EQ(asgC->value->kind, ast::NodeKind::NoneLiteral);
}

