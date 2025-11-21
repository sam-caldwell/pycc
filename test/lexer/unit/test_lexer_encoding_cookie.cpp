/***
 * Name: test_lexer_encoding_cookie
 * Purpose: Ensure encoding declaration comments on first or second line are tolerated.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

TEST(LexerEncoding, CookieOnFirstOrSecondLine) {
  const char* src1 = "# -*- coding: latin-1 -*-\n" "def f() -> int:\n  return 1\n";
  const char* src2 = "# shebang\n# coding: utf-8\n" "def g() -> int:\n  return 2\n";
  lex::Lexer L1; L1.pushString(src1, "enc1.py");
  auto t1 = L1.tokens();
  lex::Lexer L2; L2.pushString(src2, "enc2.py");
  auto t2 = L2.tokens();
  auto countKind = [](const std::vector<lex::Token>& v, lex::TokenKind k){ int n=0; for (const auto& t : v) if (t.kind==k) ++n; return n; };
  EXPECT_GE(countKind(t1, lex::TokenKind::Def), 1);
  EXPECT_GE(countKind(t2, lex::TokenKind::Def), 1);
}

