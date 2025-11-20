/***
 * Name: test_lexer_fstrings_more
 * Purpose: Exercise f-string prefixes (f/F, rf/fr) and escaped braces.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

static std::vector<lex::Token> lexAll(const char* src) {
  lex::Lexer L; L.pushString(src, "fs.py");
  return L.tokens();
}

TEST(LexerFStrings, SimpleFStringHasStringToken) {
  const char* src = "def f() -> int:\n  s = f\"hello {name}\"\n  return 0\n";
  auto toks = lexAll(src);
  bool saw = false; std::string text;
  for (const auto& t : toks) { if (t.kind == lex::TokenKind::String) { saw = true; text = t.text; break; } }
  ASSERT_TRUE(saw);
  ASSERT_FALSE(text.empty());
  // Token text should include 'f' prefix and braces content
  ASSERT_NE(text.find("f\""), std::string::npos);
  ASSERT_NE(text.find("{"), std::string::npos);
}

TEST(LexerFStrings, RawAndCombinedPrefixes) {
  const char* src = "def f() -> int:\n  a = rf\"{x}\"\n  b = fr\"{y}\"\n  return 0\n";
  auto toks = lexAll(src);
  int strings = 0; bool sawRF=false, sawFR=false;
  for (const auto& t : toks) {
    if (t.kind == lex::TokenKind::String) {
      ++strings;
      if (t.text.rfind("rf\"", 0) == 0) sawRF = true;
      if (t.text.rfind("fr\"", 0) == 0) sawFR = true;
    }
  }
  EXPECT_GE(strings, 2);
  EXPECT_TRUE(sawRF);
  EXPECT_TRUE(sawFR);
}

TEST(LexerFStrings, EscapedBracesRemainInTokenText) {
  const char* src = "def f() -> int:\n  s = f\"{{}}\"\n  return 0\n";
  auto toks = lexAll(src);
  for (const auto& t : toks) {
    if (t.kind == lex::TokenKind::String) {
      ASSERT_NE(t.text.find("{{"), std::string::npos);
      ASSERT_NE(t.text.find("}}"), std::string::npos);
      break;
    }
  }
}

