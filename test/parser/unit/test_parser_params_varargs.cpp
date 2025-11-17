/***
 * Name: test_parser_params_varargs
 * Purpose: Verify parsing of defaults, varargs, and kw-only params.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserParams, DefaultsAndVarargs) {
  const char* src =
      "def f(a: int, b=1, *args, c: float=2, **kw) -> int:\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "p.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  ASSERT_TRUE(mod);
  const auto& fn = *mod->functions[0];
  ASSERT_GE(fn.params.size(), 4u);
  EXPECT_EQ(fn.params[0].name, std::string("a"));
  EXPECT_FALSE(!fn.params[1].defaultValue);
  bool sawVarArg=false, sawCkwOnly=false;
  for (const auto& p : fn.params) {
    if (p.isVarArg) sawVarArg = true;
    if (p.name == std::string("c")) sawCkwOnly = p.isKwOnly;
  }
  EXPECT_TRUE(sawVarArg);
  EXPECT_TRUE(sawCkwOnly);
}
