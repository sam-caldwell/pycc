/***
 * Name: test_lexer_edges
 * Purpose: Cover lexer edge cases: comments/blank, indent/dedent, numbers, strings.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

static std::vector<lex::Token> lexAll(const char* src) {
  lex::Lexer L; L.pushString(src, "lex.py");
  auto toks = L.tokens();
  return toks;
}

TEST(LexerEdges, CommentsAndBlankProduceNewlinesOnly) {
  const char* src = R"PY(
# just a comment

def main() -> int:
  return 1
)PY";
  auto toks = lexAll(src);
  // Ensure the first two tokens are Newline from comment and blank
  ASSERT_GE(toks.size(), 5u);
  EXPECT_EQ(toks[0].kind, lex::TokenKind::Newline);
  EXPECT_EQ(toks[1].kind, lex::TokenKind::Newline);
}

TEST(LexerEdges, IndentDedentEmitted) {
  const char* src = R"PY(
def f() -> int:
  x = 1
  return x
def g() -> int:
  return 0
)PY";
  auto toks = lexAll(src);
  // Find at least one Indent and a matching Dedent between defs
  bool sawIndent = false, sawDedent = false;
  for (const auto& t : toks) {
    if (t.kind == lex::TokenKind::Indent) sawIndent = true;
    if (t.kind == lex::TokenKind::Dedent) sawDedent = true;
  }
  EXPECT_TRUE(sawIndent);
  EXPECT_TRUE(sawDedent);
}

TEST(LexerEdges, FloatsAndExponents) {
  auto toks = lexAll("def f() -> float:\n  return 1.0e+2\n");
  // Expect a Float token on the return expression
  bool foundFloat = false;
  for (const auto& t : toks) { if (t.kind == lex::TokenKind::Float) { foundFloat = true; break; } }
  EXPECT_TRUE(foundFloat);
}

TEST(LexerEdges, LeadingDotFloat) {
  auto toks = lexAll("def f() -> float:\n  return .5\n");
  bool foundFloat = false;
  for (const auto& t : toks) { if (t.kind == lex::TokenKind::Float) { foundFloat = true; break; } }
  EXPECT_TRUE(foundFloat);
}

TEST(LexerEdges, UnterminatedStringScansToLineEnd) {
  const char* src = R"PY(
def f() -> str:
  return "unterminated
)PY";
  auto toks = lexAll(src);
  // Ensure a String token appears
  bool foundString = false;
  for (const auto& t : toks) { if (t.kind == lex::TokenKind::String) { foundString = true; break; } }
  EXPECT_TRUE(foundString);
}

TEST(LexerEdges, KeywordsAndTypeIdents) {
  const char* src = R"PY(
def f(a: int, b: bool) -> int:
  if a and not b or True:
    return 1
  else:
    return 0
)PY";
  auto toks = lexAll(src);
  bool sawAnd=false, sawOr=false, sawNot=false, sawType=false, sawBool=false;
  for (const auto& t : toks) {
    if (t.kind == lex::TokenKind::And) sawAnd = true;
    if (t.kind == lex::TokenKind::Or) sawOr = true;
    if (t.kind == lex::TokenKind::Not) sawNot = true;
    if (t.kind == lex::TokenKind::TypeIdent && (t.text == "int" || t.text == "bool")) {
      if (t.text == "int") sawType = true; else sawBool = true;
    }
  }
  EXPECT_TRUE(sawAnd);
  EXPECT_TRUE(sawOr);
  EXPECT_TRUE(sawNot);
  EXPECT_TRUE(sawType);
  EXPECT_TRUE(sawBool);
}
