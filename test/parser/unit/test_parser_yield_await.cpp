/***
 * Name: test_parser_yield_await
 * Purpose: Verify parsing of yield/await expressions.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserYieldAwait, Basic) {
  const char* src =
      "def main() -> int:\n"
      "  x = await f()\n"
      "  y = yield 1\n"
      "  z = yield from it\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "ya.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(static_cast<const ast::AssignStmt*>(fn.body[0].get())->value->kind, ast::NodeKind::AwaitExpr);
  ASSERT_EQ(static_cast<const ast::AssignStmt*>(fn.body[1].get())->value->kind, ast::NodeKind::YieldExpr);
  ASSERT_EQ(static_cast<const ast::AssignStmt*>(fn.body[2].get())->value->kind, ast::NodeKind::YieldExpr);
}

