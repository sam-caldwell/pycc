/***
 * Name: test_lexer_unicode_ident
 * Purpose: Verify Unicode identifiers when ICU is enabled (XID rules).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

TEST(LexerUnicodeIdent, XIDStartContinue) {
#ifdef PYCC_WITH_ICU
  lex::Lexer L; L.pushString("def π():\n  return 0\n", "uid.py");
  auto toks = L.tokens();
  bool sawPi = false;
  for (const auto& t : toks) { if (t.kind == lex::TokenKind::Ident && t.text == "π") { sawPi = true; break; } }
  EXPECT_TRUE(sawPi);
#else
  GTEST_SKIP() << "ICU not enabled";
#endif
}

