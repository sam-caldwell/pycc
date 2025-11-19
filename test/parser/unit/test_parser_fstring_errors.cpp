/***
 * Name: test_parser_fstring_errors
 * Purpose: Exercise f-string parse errors: unclosed expression and single closing brace.
 */
#include <gtest/gtest.h>
#include <stdexcept>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static void expectParseError(const char* src) {
  lex::Lexer L; L.pushString(src, "perr_fstr.py");
  parse::Parser P(L);
  EXPECT_THROW({ auto m = P.parseModule(); (void)m; }, std::runtime_error);
}

TEST(ParserFStringErrors, UnclosedExpression) {
  const char* src =
      "def main() -> int:\n"
      "  s = f\"{a\"\n"
      "  return 0\n";
  expectParseError(src);
}

TEST(ParserFStringErrors, SingleClosingBrace) {
  const char* src =
      "def main() -> int:\n"
      "  s = f\"}\"\n"
      "  return 0\n";
  expectParseError(src);
}

