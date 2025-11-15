/***
 * Name: test_parser_errors
 * Purpose: Exercise parser error paths throwing runtime_error on malformed input.
 */
#include <gtest/gtest.h>
#include <stdexcept>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static void expectParseError(const char* src) {
  lex::Lexer L; L.pushString(src, "perr.py");
  parse::Parser P(L);
  EXPECT_THROW({ auto m = P.parseModule(); (void)m; }, std::runtime_error);
}

TEST(ParserErrors, MissingFunctionName) {
  const char* src = "def () -> int:\n  return 0\n";
  expectParseError(src);
}

TEST(ParserErrors, MissingRParenInParams) {
  const char* src = "def f(a: int -> int:\n  return a\n";
  expectParseError(src);
}

TEST(ParserErrors, MissingReturnTypeIdent) {
  const char* src = "def f() -> :\n  return 0\n";
  expectParseError(src);
}

TEST(ParserErrors, PipeTypeMissingSecond) {
  const char* src = "def f() -> int | :\n  return 0\n";
  expectParseError(src);
}

TEST(ParserErrors, IfMissingColonOrNewline) {
  const char* src = "def f() -> int:\n  if 1\n    return 0\n";
  expectParseError(src);
}

