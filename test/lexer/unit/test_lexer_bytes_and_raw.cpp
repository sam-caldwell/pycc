/***
 * Name: test_lexer_bytes_and_raw
 * Purpose: Verify bytes literals and raw prefixes tokenize as Bytes/String.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

static std::vector<lex::Token> lexAllBR(const char* src) {
  lex::Lexer L; L.pushString(src, "br.py");
  return L.tokens();
}

TEST(LexerBytesRaw, BytesSingleAndTriple) {
  const char* src =
      "def f() -> int:\n"
      "  a = b\"abc\"\n"
      "  b = br'xyz'\n"
      "  c = b\"\"\"hello\nworld\"\"\"\n"
      "  return 0\n";
  auto toks = lexAllBR(src);
  int bytesCnt = 0;
  for (const auto& t : toks) { if (t.kind == lex::TokenKind::Bytes) { ++bytesCnt; } }
  EXPECT_GE(bytesCnt, 2);
}

TEST(LexerBytesRaw, RawStringPrefixes) {
  const char* src =
      "def f() -> int:\n"
      "  a = r\"abc\\n\"\n"
      "  b = R'xyz'\n"
      "  return 0\n";
  auto toks = lexAllBR(src);
  int strCnt = 0;
  for (const auto& t : toks) { if (t.kind == lex::TokenKind::String) { ++strCnt; } }
  EXPECT_GE(strCnt, 2);
}

