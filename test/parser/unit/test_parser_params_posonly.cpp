/***
 * Name: test_parser_params_posonly
 * Purpose: Verify positional-only '/' divider marks earlier params as pos-only and kw-only after '*'.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/Param.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "posonly.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserParamsPosOnly, SlashAndStarDividers) {
  const char* src =
      "def f(a: int, b: int, /, c: int, *, d: int, **k) -> int:\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_GE(fn.params.size(), 4u);
  EXPECT_TRUE(fn.params[0].isPosOnly);
  EXPECT_TRUE(fn.params[1].isPosOnly);
  EXPECT_FALSE(fn.params[2].isPosOnly);
  EXPECT_TRUE(fn.params[3].isKwOnly);
}

