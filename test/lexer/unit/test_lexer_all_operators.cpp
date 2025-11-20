/***
 * Name: test_lexer_all_operators
 * Purpose: Ensure tokens exist for a broad set of operators and punctuation.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

static std::vector<lex::Token> lexOps(const char* src) {
  lex::Lexer L; L.pushString(src, "ops.py");
  return L.tokens();
}

TEST(LexerOps, MajorOperatorsPresent) {
  const char* src =
      "def f() -> int:\n"
      "  a = 1 + 2 - 3 * 4 / 5 // 2 % 7 ** 2\n"
      "  b = a << 1 >> 2 & 3 | 4 ^ 5\n"
      "  c = a == b != 0 < 1 <= 2 > 3 >= 4\n"
      "  d = (a) [0] {1:2}\n"
      "  e = a and b or not c\n"
      "  return d\n";
  auto toks = lexOps(src);
  bool sawAdd=false, sawPow=false, sawLShift=false, sawRShift=false, sawAnd=false, sawOr=false, sawNot=false;
  for (const auto& t : toks) {
    if (t.kind == lex::TokenKind::Plus) sawAdd = true;
    if (t.kind == lex::TokenKind::StarStar) sawPow = true;
    if (t.kind == lex::TokenKind::LShift) sawLShift = true;
    if (t.kind == lex::TokenKind::RShift) sawRShift = true;
    if (t.kind == lex::TokenKind::And) sawAnd = true;
    if (t.kind == lex::TokenKind::Or) sawOr = true;
    if (t.kind == lex::TokenKind::Not) sawNot = true;
  }
  EXPECT_TRUE(sawAdd);
  EXPECT_TRUE(sawPow);
  EXPECT_TRUE(sawLShift);
  EXPECT_TRUE(sawRShift);
  EXPECT_TRUE(sawAnd);
  EXPECT_TRUE(sawOr);
  EXPECT_TRUE(sawNot);
}

