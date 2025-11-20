/***
 * Name: test_parser_async_await_yield
 * Purpose: Verify await and yield forms parse in appropriate contexts.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/YieldExpr.h"
#include "ast/AwaitExpr.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "ay.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserAsyncAwaitYield, AwaitInAsyncDef) {
  const char* src =
      "async def g() -> int:\n"
      "  return await h()\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
  ASSERT_TRUE(ret->value);
  EXPECT_EQ(ret->value->kind, ast::NodeKind::AwaitExpr);
}

TEST(ParserAsyncAwaitYield, YieldAndYieldFrom) {
  const char* src =
      "def gen() -> int:\n"
      "  yield from xs\n"
      "def gen2() -> int:\n"
      "  yield 1\n";
  auto mod = parseSrc(src);
  const auto& f1 = *mod->functions[0];
  const auto* y1 = static_cast<const ast::ExprStmt*>(f1.body[0].get());
  ASSERT_EQ(y1->value->kind, ast::NodeKind::YieldExpr);
  const auto& f2 = *mod->functions[1];
  const auto* y2 = static_cast<const ast::ExprStmt*>(f2.body[0].get());
  ASSERT_EQ(y2->value->kind, ast::NodeKind::YieldExpr);
}

