/***
 * Name: test_lexer_operators_all
 * Purpose: Cover all operator and punctuation tokens produced by the lexer.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

static std::vector<lex::Token> lexAll(const char* src) {
  lex::Lexer L; L.pushString(src, "ops.py");
  return L.tokens();
}

TEST(LexerOperators, AllBinaryAndAssignVariants) {
  const char* src =
      "def f() -> int\n"
      "  a = 1\n"
      "  a += 1\n"
      "  a -= 1\n"
      "  a *= 2\n"
      "  a **= 3\n"
      "  a /= 4\n"
      "  a //= 5\n"
      "  a %= 6\n"
      "  a <<= 1\n"
      "  a >>= 2\n"
      "  a &= 3\n"
      "  a ^= 4\n"
      "  a |= 5\n"
      "  a == 5\n"
      "  a != 6\n"
      "  a < 7\n"
      "  a <= 7\n"
      "  a > 7\n"
      "  a >= 7\n"
      "  a << 1\n"
      "  a >> 1\n"
      "  a & b\n"
      "  a | b\n"
      "  a ^ b\n"
      "  a ~ b\n"
      "  ( [ { 1 }, ] )\n"
      "  .5\n"
      "  ...\n"
      "  a.b\n"
      "  a -> b\n"
      "  , : @\n"
      "  return 0\n";
  auto toks = lexAll(src);
  using TK = lex::TokenKind;
  auto has = [&](TK k){ for (const auto& t : toks) if (t.kind == k) return true; return false; };
  EXPECT_TRUE(has(TK::Equal));
  EXPECT_TRUE(has(TK::PlusEqual));
  EXPECT_TRUE(has(TK::MinusEqual));
  EXPECT_TRUE(has(TK::StarEqual));
  EXPECT_TRUE(has(TK::StarStarEqual));
  EXPECT_TRUE(has(TK::SlashEqual));
  EXPECT_TRUE(has(TK::SlashSlashEqual));
  EXPECT_TRUE(has(TK::PercentEqual));
  EXPECT_TRUE(has(TK::LShiftEqual));
  EXPECT_TRUE(has(TK::RShiftEqual));
  EXPECT_TRUE(has(TK::AmpEqual));
  EXPECT_TRUE(has(TK::CaretEqual));
  EXPECT_TRUE(has(TK::PipeEqual));
  EXPECT_TRUE(has(TK::EqEq));
  EXPECT_TRUE(has(TK::NotEq));
  EXPECT_TRUE(has(TK::Lt));
  EXPECT_TRUE(has(TK::Le));
  EXPECT_TRUE(has(TK::Gt));
  EXPECT_TRUE(has(TK::Ge));
  EXPECT_TRUE(has(TK::LShift));
  EXPECT_TRUE(has(TK::RShift));
  EXPECT_TRUE(has(TK::Amp));
  EXPECT_TRUE(has(TK::Pipe));
  EXPECT_TRUE(has(TK::Caret));
  EXPECT_TRUE(has(TK::Tilde));
  EXPECT_TRUE(has(TK::LParen));
  EXPECT_TRUE(has(TK::RParen));
  EXPECT_TRUE(has(TK::LBracket));
  EXPECT_TRUE(has(TK::RBracket));
  EXPECT_TRUE(has(TK::LBrace));
  EXPECT_TRUE(has(TK::RBrace));
  EXPECT_TRUE(has(TK::Float));
  EXPECT_TRUE(has(TK::Ellipsis));
  EXPECT_TRUE(has(TK::Dot));
  EXPECT_TRUE(has(TK::Arrow));
  EXPECT_TRUE(has(TK::Comma));
  EXPECT_TRUE(has(TK::Colon));
  EXPECT_TRUE(has(TK::At));
}

