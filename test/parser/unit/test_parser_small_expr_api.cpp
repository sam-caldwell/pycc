/***
 * Name: test_parser_small_expr_api
 * Purpose: Cover Parser::parseSmallExprFromString() public static helper.
 */
#include <gtest/gtest.h>
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserSmallExprAPI, ParsesConditionalExpression) {
  auto e = parse::Parser::parseSmallExprFromString("1 if 0 else 2", "<t>");
  ASSERT_NE(e, nullptr);
  // Shape-only assertion: expression is produced
}

