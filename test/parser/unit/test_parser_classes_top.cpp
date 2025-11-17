/***
 * Name: test_parser_classes_top
 * Purpose: Ensure top-level class decorators and base list parsing are captured on Module.classes.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/ClassDef.h"

using namespace pycc;

TEST(ParserClassesTop, TopLevelDecoratorsAndBases) {
  const char* src =
      "@dec1\n"
      "@dec2(3)\n"
      "class C(A, B):\n"
      "  pass\n";
  lex::Lexer L; L.pushString(src, "clstop.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  ASSERT_EQ(mod->classes.size(), 1u);
  const auto* cls = mod->classes[0].get();
  ASSERT_EQ(cls->decorators.size(), 2u);
  ASSERT_EQ(cls->bases.size(), 2u);
}

