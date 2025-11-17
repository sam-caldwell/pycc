/***
 * Name: test_parser_imag_literal
 * Purpose: Verify imaginary numeric literals (e.g., 3j) are parsed.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserImag, BasicAndBinary) {
  const char* src =
      "def main() -> int:\n"
      "  a = 3j\n"
      "  b = 1 + 2j\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "imag.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  const auto& fn = *mod->functions[0];
  const auto* asgA = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asgA->value->kind, ast::NodeKind::ImagLiteral);
  const auto* asgB = static_cast<const ast::AssignStmt*>(fn.body[1].get());
  ASSERT_EQ(asgB->value->kind, ast::NodeKind::BinaryExpr);
}

