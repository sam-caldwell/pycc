/***
 * Name: test_parser_with_async
 * Purpose: Verify parsing of with/async with and multiple with-items with 'as' bindings.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "with.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserWith, MultipleItemsAndAs) {
  const char* src =
      "def main() -> int:\n"
      "  with a() as x, b() as y:\n"
      "    return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::WithStmt);
  const auto* ws = static_cast<const ast::WithStmt*>(fn.body[0].get());
  ASSERT_EQ(ws->items.size(), 2u);
  EXPECT_EQ(ws->items[0]->asName, std::string("x"));
  EXPECT_EQ(ws->items[1]->asName, std::string("y"));
}

TEST(ParserWith, AsyncWith) {
  const char* src =
      "async def main() -> int:\n"
      "  async with c() as z:\n"
      "    return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::WithStmt);
}

