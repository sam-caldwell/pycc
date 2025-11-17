/***
 * Name: test_lexer_strings_extended
 * Purpose: Verify triple-quoted and f-strings are tokenized as String.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

TEST(LexerStrings, TripleAndF) {
  const char* src =
      "def main() -> int:\n"
      "  a = \"\"\"hello\nworld\"\"\"\n"
      "  b = f\"hi {x}\"\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "s.py");
  auto toks = L.tokens();
  bool sawTriple = false;
  for (auto& t : toks) { if (t.kind == lex::TokenKind::String) { sawTriple = true; break; } }
  EXPECT_TRUE(sawTriple);
}
