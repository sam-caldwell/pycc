/***
 * Name: test_parser_decorators_advanced
 * Purpose: Verify decorators accept dotted names and call expressions.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserDecorators, DottedAndCallDecorators) {
  const char* src =
      "@pkg.dec(1, 2)\n"
      "@other\n"
      "def f() -> int:\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "dec.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.decorators.size(), 2u);
}

