/***
 * Name: test_lexer_typeidents_more
 * Purpose: Ensure more type identifiers are classified as TypeIdent.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

static std::vector<lex::Token> lexAll(const char* src) {
  lex::Lexer L; L.pushString(src, "types.py");
  return L.tokens();
}

TEST(LexerTypeIdents, MoreKnownTypes) {
  const char* src =
      "def f(a: float, b: str, c: tuple, d: list, e: dict, f: Optional, g: Union) -> None:\n"
      "  return None\n";
  auto toks = lexAll(src);
  int count = 0; bool sawNone=false;
  for (const auto& t : toks) {
    if (t.kind == lex::TokenKind::TypeIdent) {
      ++count;
      if (t.text == "None") sawNone = true;
    }
  }
  EXPECT_GE(count, 7);
  EXPECT_TRUE(sawNone);
}

