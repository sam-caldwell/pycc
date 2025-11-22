/***
 * Name: test_lexer_strings_edges2
 * Purpose: Exercise string prefixes, f/raw combos, triple quotes, and unterminated cases.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace pycc;

static std::vector<lex::Token> lexAll(const char* src) {
  lex::Lexer L; L.pushString(src, "str2.py");
  return L.tokens();
}

TEST(LexerStrings, PrefixesTripleAndUnterminated) {
  const char* src =
      "def f() -> int\n"
      "  s1 = 'abc\\'def'\n"        // escapes inside normal string
      "  s2 = u'hello'\n"             // unicode prefix tolerated
      "  s3 = b'bytes'\n"             // bytes simple
      "  s4 = rb'raw bytes'\n"         // raw+bytes
      "  s5 = br'raw bytes2'\n"        // bytes+raw
      "  s6 = bf'not bytes but f'\n"   // b+f together => String token
      "  s7 = f'val {x}'\n"            // f-string kept as String token
      "  s8 = '''triple'''\n"         // triple-quoted (single quotes)
      "  s9 = r\"\"\"raw triple\"\"\"\n" // raw triple-quoted
      "  s10 = b\"\"\"bytes triple\"\"\"\n" // bytes triple-quoted
      "  s11 = \"unterminated\n"        // unterminated normal string goes to EOL
      "  return 0\n";
  auto toks = lexAll(src);
  int strings = 0, bytes = 0;
  bool sawBFasString=false, sawF=false, sawTriple=false, sawRawTriple=false, sawBytesTriple=false, sawUnterminated=false;
  for (const auto& t : toks) {
    if (t.kind == lex::TokenKind::String) {
      ++strings;
      if (t.text.rfind("bf'", 0) == 0) sawBFasString = true;
      if (t.text.rfind("f'", 0) == 0) sawF = true;
      if (t.text == "'''") sawTriple = true;
      if (t.text == "r\"\"\"") sawRawTriple = true;
      if (t.text.find("\"unterminated") != std::string::npos) sawUnterminated = true;
    }
    if (t.kind == lex::TokenKind::Bytes) {
      ++bytes;
      if (t.text == "b\"\"\"") sawBytesTriple = true;
    }
  }
  EXPECT_GE(strings, 5);
  EXPECT_GE(bytes, 2);
  EXPECT_TRUE(sawBFasString);
  EXPECT_TRUE(sawF);
  EXPECT_TRUE(sawTriple);
  EXPECT_TRUE(sawRawTriple);
  EXPECT_TRUE(sawBytesTriple);
  EXPECT_TRUE(sawUnterminated);
}

