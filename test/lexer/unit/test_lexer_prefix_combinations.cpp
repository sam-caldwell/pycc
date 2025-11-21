/***
 * Name: test_lexer_prefix_combinations
 * Purpose: Verify all string/bytes prefix combinations tokenize correctly.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

static std::vector<lex::Token> lexAll(const char* src) {
  lex::Lexer L; L.pushString(src, "pref.py");
  return L.tokens();
}

TEST(LexerPrefixes, SingleLetterPrefixes) {
  const char* src =
      "def f() -> int:\n"
      "  a = r'raw'\n"
      "  b = b\"bytes\"\n"
      "  c = f\"{x}\"\n"
      "  u = u'legacy'\n"
      "  return 0\n";
  auto toks = lexAll(src);
  int strings = 0, bytes = 0;
  for (const auto& t : toks) {
    if (t.kind == lex::TokenKind::String) ++strings;
    if (t.kind == lex::TokenKind::Bytes) ++bytes;
  }
  EXPECT_GE(strings, 3);
  EXPECT_GE(bytes, 1);
}

TEST(LexerPrefixes, TwoLetterCombos) {
  const char* src =
      "def g() -> int:\n"
      "  a = rf\"{x}\"\n"
      "  b = fr\"{y}\"\n"
      "  c = rb'xyz'\n"
      "  d = br\"q\\n\"\n"
      "  return 0\n";
  auto toks = lexAll(src);
  bool sawRF=false, sawFR=false, sawRB=false, sawBR=false;
  for (const auto& t : toks) {
    if (t.kind == lex::TokenKind::String && t.text.rfind("rf\"", 0) == 0) sawRF = true;
    if (t.kind == lex::TokenKind::String && t.text.rfind("fr\"", 0) == 0) sawFR = true;
    if (t.kind == lex::TokenKind::Bytes && (t.text.rfind("rb'", 0) == 0 || t.text.rfind("rb\"", 0) == 0)) sawRB = true;
    if (t.kind == lex::TokenKind::Bytes && (t.text.rfind("br'", 0) == 0 || t.text.rfind("br\"", 0) == 0)) sawBR = true;
  }
  EXPECT_TRUE(sawRF);
  EXPECT_TRUE(sawFR);
  EXPECT_TRUE(sawRB);
  EXPECT_TRUE(sawBR);
}

TEST(LexerPrefixes, TripleQuotedCombos) {
  const char* src =
      "def h() -> int:\n"
      "  a = r\"\"\"hello\nworld\"\"\"\n"
      "  b = b\"\"\"abc\nxyz\"\"\"\n"
      "  return 0\n";
  auto toks = lexAll(src);
  int strings = 0, bytes = 0;
  for (const auto& t : toks) {
    if (t.kind == lex::TokenKind::String) ++strings;
    if (t.kind == lex::TokenKind::Bytes) ++bytes;
  }
  EXPECT_GE(strings, 1);
  EXPECT_GE(bytes, 1);
}

