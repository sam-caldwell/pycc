/***
 * Name: test_lexer_indent_streaming
 * Purpose: Cover CRLF handling, blank/comment lines, INDENT/DEDENT emission, and streaming refill/peek paths.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

TEST(LexerIndent, CRLFBlankCommentAndStreaming) {
  const char* src =
      "def f() -> int\r\n"
      "  x = 1\r\n"
      "  # comment only\r\n"
      "  \r\n"
      "    y = 2\r\n"
      "  z = 3\r\n";
  lex::Lexer L; L.pushString(src, "crlf.py");

  // Exercise peek with lookahead and streaming next
  const auto& la0 = L.peek(0);
  const auto& la1 = L.peek(1);
  (void)la0; (void)la1;

  int newlines = 0, indents = 0, dedents = 0;
  for (;;) {
    lex::Token t = L.next();
    if (t.kind == lex::TokenKind::Newline) {
      ++newlines;
    }
    if (t.kind == lex::TokenKind::Indent) ++indents;
    if (t.kind == lex::TokenKind::Dedent) ++dedents;
    if (t.kind == lex::TokenKind::End) break;
  }
  EXPECT_GE(newlines, 5); // includes comment+blank line newlines
  EXPECT_GE(indents, 1);
  EXPECT_GE(dedents, 1);
}
