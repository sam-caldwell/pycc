/***
 * Name: test_lexer_fileinput
 * Purpose: Cover FileInput getline() true/false paths via pushFile; lexing simple file and missing file case.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include <cstdio>

using namespace pycc;

TEST(LexerFileInput, MissingFileProducesEOF) {
  lex::Lexer L;
  L.pushFile("__pycc_missing_file__");
  auto toks = L.tokens();
  ASSERT_FALSE(toks.empty());
  EXPECT_EQ(toks.back().kind, lex::TokenKind::End);
}

TEST(LexerFileInput, ReadsSimpleFile) {
  const char* path = "_lex_tmp.py";
  {
    FILE* f = std::fopen(path, "wb");
    ASSERT_NE(f, nullptr);
    const char* src = "def f():\n  return 1\n";
    std::fwrite(src, 1, std::strlen(src), f);
    std::fclose(f);
  }
  lex::Lexer L; L.pushFile(path);
  auto toks = L.tokens();
  // Should include at least a Def and Return token somewhere
  bool sawDef = false, sawReturn = false;
  for (const auto& t : toks) { if (t.kind == lex::TokenKind::Def) sawDef = true; if (t.kind == lex::TokenKind::Return) sawReturn = true; }
  EXPECT_TRUE(sawDef);
  EXPECT_TRUE(sawReturn);
  std::remove(path);
}

