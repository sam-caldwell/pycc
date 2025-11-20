/***
 * Name: test_parser_async_for_comprehension
 * Purpose: Verify 'async for' inside comprehensions parses and marks isAsync.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/Comprehension.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "acomp.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserAsyncForComp, ListCompAsyncForAccepted) {
  const char* src =
      "async def f(xs: list) -> list:\n"
      "  return [x async for x in xs]\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
  ASSERT_TRUE(ret->value);
  ASSERT_EQ(ret->value->kind, ast::NodeKind::ListComp);
  const auto* lc = static_cast<const ast::ListComp*>(ret->value.get());
  ASSERT_FALSE(lc->fors.empty());
  EXPECT_TRUE(lc->fors[0].isAsync);
}

