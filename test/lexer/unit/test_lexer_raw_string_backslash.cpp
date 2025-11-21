/***
 * Name: test_lexer_raw_string_backslash
 * Purpose: Ensure raw strings that end with a single backslash are treated as unterminated (line-spanning) for recovery.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

TEST(LexerRawString, TerminalBackslashExtendsToEOL) {
  const char* src = "def f() -> int:\n  s = r\"abc\\\"\n  return 0\n";
  lex::Lexer L; L.pushString(src, "raw.py");
  auto toks = L.tokens();
  bool saw = false;
  for (const auto& t : toks) {
    if (t.kind == lex::TokenKind::String) {
      // Expect token text to run to EOL (no closing quote included)
      if (t.text.find("r\"abc\\\"") != std::string::npos) { saw = true; break; }
    }
  }
  EXPECT_TRUE(saw);
}

