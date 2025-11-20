/***
 * Name: test_lexer_comparators
 * Purpose: Verify comparator tokens: !=, <=, >=, is, in.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

static std::vector<lex::Token> lexAll(const char* src) {
  lex::Lexer L; L.pushString(src, "cmp.py");
  return L.tokens();
}

TEST(LexerComparators, NotEqLeGeIsIn) {
  const char* src =
      "def f(a: int, b: int, xs: list) -> bool:\n"
      "  return (a != b) and (a <= b) and (a >= b) and (a is b) and (a in xs) and (not a in xs)\n";
  auto toks = lexAll(src);
  using TK = lex::TokenKind;
  bool sawNe=false, sawLe=false, sawGe=false, sawIs=false, sawIn=false;
  for (const auto& t : toks) {
    if (t.kind == TK::NotEq) sawNe = true;
    if (t.kind == TK::Le) sawLe = true;
    if (t.kind == TK::Ge) sawGe = true;
    if (t.kind == TK::Is) sawIs = true;
    if (t.kind == TK::In) sawIn = true;
  }
  EXPECT_TRUE(sawNe);
  EXPECT_TRUE(sawLe);
  EXPECT_TRUE(sawGe);
  EXPECT_TRUE(sawIs);
  EXPECT_TRUE(sawIn);
}

