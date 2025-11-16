/***
 * Name: test_parser_globals_nonlocal_assert
 * Purpose: Verify parsing of global/nonlocal/assert statements.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserGlobalsNonlocalAssert, Basic) {
  const char* src =
      "def main() -> int:\n"
      "  global a, b\n"
      "  nonlocal c\n"
      "  assert a, 'msg'\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "gna.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::GlobalStmt);
  ASSERT_EQ(fn.body[1]->kind, ast::NodeKind::NonlocalStmt);
  ASSERT_EQ(fn.body[2]->kind, ast::NodeKind::AssertStmt);
}

