/***
 * Name: test_lexer_tostring_all
 * Purpose: Cover to_string(TokenKind) for all valid kinds and the default path.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include <string>

using namespace pycc;

TEST(LexerToString, AllKindsAndUnknown) {
  using TK = lex::TokenKind;
  // Iterate a safe integer range that covers all enum values defined.
  // TokenKind values are contiguous by declaration order.
  for (int i = static_cast<int>(TK::End); i <= static_cast<int>(TK::Ellipsis); ++i) {
    const char* s = lex::to_string(static_cast<TK>(i));
    ASSERT_NE(s, nullptr);
    ASSERT_NE(std::string(s).size(), 0u);
  }
  // Also cover the default Unknown branch by casting an invalid value
  const char* u = lex::to_string(static_cast<TK>(-1));
  ASSERT_STREQ(u, "Unknown");
}

