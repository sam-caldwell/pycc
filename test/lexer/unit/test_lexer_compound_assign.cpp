/***
 * Name: test_lexer_compound_assign
 * Purpose: Verify compound assignment tokens and walrus operator.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

static std::vector<lex::Token> lexAll(const char* src) {
  lex::Lexer L; L.pushString(src, "ops.py");
  return L.tokens();
}

TEST(LexerCompound, AssignmentsAndWalrus) {
  const char* src =
      "def f() -> int:\n"
      "  a += 1\n"
      "  a -= 1\n"
      "  a *= 2\n"
      "  a /= 2\n"
      "  a //= 2\n"
      "  a %= 3\n"
      "  a <<= 1\n"
      "  a >>= 1\n"
      "  a &= 1\n"
      "  a ^= 1\n"
      "  a |= 1\n"
      "  b := 5\n"
      "  return a\n";
  auto toks = lexAll(src);
  int plusEq=0, minusEq=0, starEq=0, slashEq=0, fslashEq=0, percentEq=0, lshEq=0, rshEq=0, ampEq=0, caretEq=0, pipeEq=0, walrus=0;
  for (const auto& t : toks) {
    using TK = lex::TokenKind;
    switch (t.kind) {
      case TK::PlusEqual: ++plusEq; break;
      case TK::MinusEqual: ++minusEq; break;
      case TK::StarEqual: ++starEq; break;
      case TK::SlashEqual: ++slashEq; break;
      case TK::SlashSlashEqual: ++fslashEq; break;
      case TK::PercentEqual: ++percentEq; break;
      case TK::LShiftEqual: ++lshEq; break;
      case TK::RShiftEqual: ++rshEq; break;
      case TK::AmpEqual: ++ampEq; break;
      case TK::CaretEqual: ++caretEq; break;
      case TK::PipeEqual: ++pipeEq; break;
      case TK::ColonEqual: ++walrus; break;
      default: break;
    }
  }
  EXPECT_EQ(plusEq, 1);
  EXPECT_EQ(minusEq, 1);
  EXPECT_EQ(starEq, 1);
  EXPECT_EQ(slashEq, 1);
  EXPECT_EQ(fslashEq, 1);
  EXPECT_EQ(percentEq, 1);
  EXPECT_EQ(lshEq, 1);
  EXPECT_EQ(rshEq, 1);
  EXPECT_EQ(ampEq, 1);
  EXPECT_EQ(caretEq, 1);
  EXPECT_EQ(pipeEq, 1);
  EXPECT_EQ(walrus, 1);
}

