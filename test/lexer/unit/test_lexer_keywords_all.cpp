/***
 * Name: test_lexer_keywords_all
 * Purpose: Ensure a broad set of Python keywords are recognized.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

static std::vector<lex::Token> lexAll(const char* src) {
  lex::Lexer L; L.pushString(src, "kw.py");
  return L.tokens();
}

TEST(LexerKeywords, MajorKeywords) {
  const char* src =
      "async def g() -> int:\n"
      "  await g()\n"
      "  del g\n"
      "  pass\n"
      "  break\n"
      "  continue\n"
      "  try:\n"
      "    x = 1\n"
      "  except Exception as e:\n"
      "    x = 2\n"
      "  finally:\n"
      "    x = 3\n"
      "  with x as y:\n"
      "    x = 4\n"
      "  match x:\n"
      "    case 1:\n"
      "      x = 5\n"
      "  import sys\n"
      "  from sys import path\n"
      "  class C:\n"
      "    pass\n"
      "  global x\n"
      "  nonlocal y\n"
      "  lambda: 0\n"
      "  return 0\n";
  auto toks = lexAll(src);
  using TK = lex::TokenKind;
  auto has = [&](TK k){ for (const auto& t : toks) if (t.kind == k) return true; return false; };
  EXPECT_TRUE(has(TK::Async));
  EXPECT_TRUE(has(TK::Await));
  EXPECT_TRUE(has(TK::Del));
  EXPECT_TRUE(has(TK::Pass));
  EXPECT_TRUE(has(TK::Break));
  EXPECT_TRUE(has(TK::Continue));
  EXPECT_TRUE(has(TK::Try));
  EXPECT_TRUE(has(TK::Except));
  EXPECT_TRUE(has(TK::Finally));
  EXPECT_TRUE(has(TK::With));
  EXPECT_TRUE(has(TK::As));
  EXPECT_TRUE(has(TK::Match));
  EXPECT_TRUE(has(TK::Case));
  EXPECT_TRUE(has(TK::Import));
  EXPECT_TRUE(has(TK::From));
  EXPECT_TRUE(has(TK::Class));
  EXPECT_TRUE(has(TK::Global));
  EXPECT_TRUE(has(TK::Nonlocal));
  EXPECT_TRUE(has(TK::Lambda));
}

