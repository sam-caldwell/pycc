/***
 * Name: test_lexer_namedexpr
 * Purpose: Verify scanning of ':=' as ColonEqual and not two tokens.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

TEST(LexerNamedExpr, ColonEqualToken) {
  lex::Lexer L; L.pushString("x := 3\n", "t.py");
  auto toks = L.tokens();
  bool sawColonEq = false;
  for (const auto& t : toks) {
    if (t.kind == lex::TokenKind::ColonEqual) { sawColonEq = true; break; }
  }
  ASSERT_TRUE(sawColonEq);
}

