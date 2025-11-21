/***
 * Name: test_lexer_numbers_edges
 * Purpose: Cover numeric literal variants with underscores and bases, incl. floats and imaginary.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

static std::vector<lex::Token> lexAll(const char* src) {
  lex::Lexer L; L.pushString(src, "num.py");
  return L.tokens();
}

TEST(LexerNumbers, BasesAndUnderscores) {
  const char* src =
      "def f() -> int:\n"
      "  a = 0b1_0_1\n"
      "  b = 0o7_1\n"
      "  c = 0xDE_AD_BE_EF\n"
      "  return 0\n";
  auto toks = lexAll(src);
  int ints = 0;
  for (const auto& t : toks) if (t.kind == lex::TokenKind::Int) ++ints;
  EXPECT_GE(ints, 3);
}

TEST(LexerNumbers, FloatsAndExponentUnderscoresAndImag) {
  const char* src =
      "def g() -> int:\n"
      "  x = 1_2_3.4_5_6e1_0\n"
      "  y = .5_0j\n"
      "  z = 10_0j\n"
      "  return 0\n";
  auto toks = lexAll(src);
  bool sawFloat = false, sawImag = false;
  for (const auto& t : toks) {
    if (t.kind == lex::TokenKind::Float) sawFloat = true;
    if (t.kind == lex::TokenKind::Imag) sawImag = true;
  }
  EXPECT_TRUE(sawFloat);
  EXPECT_TRUE(sawImag);
}

