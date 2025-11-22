/***
 * Name: test_lexer_tokens_finalize
 * Purpose: Ensure calling tokens() multiple times uses finalized fast path.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

TEST(LexerMisc, TokensFinalizeIdempotent) {
  const char* src = "x = 1\n";
  lex::Lexer L; L.pushString(src, "final.py");
  auto t1 = L.tokens();
  auto t2 = L.tokens();
  ASSERT_GE(t1.size(), 2);
  ASSERT_EQ(t1.size(), t2.size());
  for (size_t i = 0; i < t1.size(); ++i) {
    EXPECT_EQ(static_cast<int>(t1[i].kind), static_cast<int>(t2[i].kind));
    EXPECT_EQ(t1[i].text, t2[i].text);
  }
}

