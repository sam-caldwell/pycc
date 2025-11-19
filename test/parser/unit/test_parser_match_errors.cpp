/***
 * Name: test_parser_match_errors
 * Purpose: Exercise parser errors in match patterns: unsupported mapping key and positional after keyword in class pattern.
 */
#include <gtest/gtest.h>
#include <stdexcept>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static void expectParseError(const char* src) {
  lex::Lexer L; L.pushString(src, "perr_match.py");
  parse::Parser P(L);
  EXPECT_THROW({ auto m = P.parseModule(); (void)m; }, std::runtime_error);
}

TEST(ParserMatchErrors, UnsupportedMappingKeyInPattern) {
  const char* src =
      "def main() -> int:\n"
      "  match x:\n"
      "    case {[1]: v}:\n"
      "      pass\n"
      "  return 0\n";
  expectParseError(src);
}

TEST(ParserMatchErrors, PositionalAfterKeywordInClassPatternRejected) {
  const char* src =
      "def main() -> int:\n"
      "  match x:\n"
      "    case C(x=a, b):\n"
      "      pass\n"
      "  return 0\n";
  expectParseError(src);
}

