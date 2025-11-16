/***
 * Name: test_parser_augassign_raise
 * Purpose: Verify parsing of augmented assignment and raise.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserAugAssignRaise, Basic) {
  const char* src =
      "def main() -> int:\n"
      "  x = 0\n"
      "  x += 1\n"
      "  raise ValueError('err')\n"
      "  return x\n";
  lex::Lexer L; L.pushString(src, "ar.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[1]->kind, ast::NodeKind::AugAssignStmt);
  ASSERT_EQ(fn.body[2]->kind, ast::NodeKind::RaiseStmt);
}

