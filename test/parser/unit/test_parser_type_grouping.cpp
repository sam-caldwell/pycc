/***
 * Name: test_parser_type_grouping
 * Purpose: Accept parenthesized type groupings in annotations and return types (shape-only).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "tg.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserTypeGrouping, ReturnParenthesizedUnionShape) {
  const char* src =
      "def f() -> (int | None):\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  // Shape-only: we record the first type token
  EXPECT_EQ(fn.returnType, ast::TypeKind::Int);
}

TEST(ParserTypeGrouping, ParamParenthesizedUnionShape) {
  const char* src =
      "def f(x: (int | float)) -> int:\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.params.size(), 1u);
  // Shape-only: we record the first type token
  EXPECT_EQ(fn.params[0].type, ast::TypeKind::Int);
}

