/***
 * Name: test_parser_arg_errors
 * Purpose: Exercise parseArgList error: positional argument after keyword argument.
 */
#include <gtest/gtest.h>
#include <stdexcept>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static void expectParseError(const char* src) {
  lex::Lexer L; L.pushString(src, "perr_args.py");
  parse::Parser P(L);
  EXPECT_THROW({ auto m = P.parseModule(); (void)m; }, std::runtime_error);
}

TEST(ParserArgErrors, PositionalAfterKeywordInCallRejected) {
  const char* src =
      "def main() -> int:\n"
      "  x = f(a=1, 2)\n"
      "  return 0\n";
  expectParseError(src);
}

