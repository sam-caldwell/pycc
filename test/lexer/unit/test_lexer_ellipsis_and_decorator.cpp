/***
 * Name: test_lexer_ellipsis_and_decorator
 * Purpose: Verify ellipsis and decorator tokens appear.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

static std::vector<lex::Token> lexAll(const char* src) {
  lex::Lexer L; L.pushString(src, "ell.py");
  return L.tokens();
}

TEST(LexerMisc, EllipsisAndDecorator) {
  const char* src =
      "@dec\n"
      "def f() -> int:\n"
      "  return ...\n";
  auto toks = lexAll(src);
  bool sawAt=false, sawEll=false;
  for (const auto& t : toks) { if (t.kind == lex::TokenKind::At) sawAt = true; if (t.kind == lex::TokenKind::Ellipsis) sawEll = true; }
  EXPECT_TRUE(sawAt);
  EXPECT_TRUE(sawEll);
}

