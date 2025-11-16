/***
 * Name: test_parser_invalid_targets
 * Purpose: Verify invalid assignment targets throw parse errors.
 */
#include <gtest/gtest.h>
#include <stdexcept>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserInvalidTargets, InvalidTargetBinaryExpr) {
  const char* src =
      "def main() -> int:\n"
      "  a + b = 3\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "bad.py");
  parse::Parser P(L);
  EXPECT_THROW({ auto m = P.parseModule(); (void)m; }, std::runtime_error);
}

