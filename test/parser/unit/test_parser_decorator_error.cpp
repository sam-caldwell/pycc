/***
 * Name: test_parser_decorator_error
 * Purpose: Ensure decorators not followed by def/class raise a parse error.
 */
#include <gtest/gtest.h>
#include <stdexcept>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserDecoratorError, DecoratorMustPrecedeDefOrClass) {
  const char* src =
      "def main() -> int:\n"
      "  @dec\n"
      "  x = 1\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "perr_deco.py");
  parse::Parser P(L);
  EXPECT_THROW({ auto m = P.parseModule(); (void)m; }, std::runtime_error);
}

