/***
 * Name: test_lexer_numbers_trailing_underscores
 * Purpose: Ensure trailing underscores are trimmed in numeric scanning and imag for based literals.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

static std::vector<lex::Token> lexAll(const char* src) {
  lex::Lexer L; L.pushString(src, "num2.py");
  return L.tokens();
}

TEST(LexerNumbers, TrailingUnderscoresAndImagBases) {
  const char* src =
      "def g() -> int\n"
      "  a = 1_\n"       // trims to 1
      "  b = 1e+\n"       // exponent without digits -> int 1 and ident e
      "  c = 1e1_\n"      // trims trailing underscore in exponent
      "  d = 0b1_\n"      // trims to 0b1
      "  e = 0o7_\n"
      "  f = 0xF_\n"
      "  g = 0b1j\n"      // imag with binary base
      "  return 0\n";
  auto toks = lexAll(src);
  using TK = lex::TokenKind;
  int ints = 0, floats = 0, imags = 0;
  for (const auto& t : toks) {
    if (t.kind == TK::Int) ++ints;
    if (t.kind == TK::Float) ++floats;
    if (t.kind == TK::Imag) ++imags;
  }
  EXPECT_GE(ints, 5);
  EXPECT_GE(floats, 1);
  EXPECT_GE(imags, 1);
}

