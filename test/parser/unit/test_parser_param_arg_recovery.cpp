/***
 * Name: test_parser_param_arg_recovery
 * Purpose: Trigger recovery paths in parameter and argument lists to lift coverage.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static void expectParseError(const char* src) {
  lex::Lexer L; L.pushString(src, "pargs.py");
  parse::Parser P(L);
  EXPECT_THROW({ (void)P.parseModule(); }, std::runtime_error);
}

TEST(ParserParamRecovery, MissingNameBetweenCommas) {
  const char* src =
      "def f(a: int, , b: int) -> int:\n"
      "  return 0\n";
  expectParseError(src);
}

TEST(ParserArgRecovery, EmptyArgumentBetweenCommas) {
  const char* src =
      "def f(a: int, b: int) -> int:\n"
      "  return 0\n"
      "def g() -> int:\n"
      "  f(1, , 3)\n"
      "  return 0\n";
  expectParseError(src);
}

