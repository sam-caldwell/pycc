/***
 * Name: test_parser_closures
 * Purpose: Ensure nested def captures (shape) and references to outer vars appear as Names in inner function.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/DefStmt.h"
#include "ast/ReturnStmt.h"
#include "ast/Name.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "clos.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserClosures, NestedDefReferencesOuterVar) {
  const char* src =
      "def outer() -> int:\n"
      "  y = 5\n"
      "  def inner() -> int:\n"
      "    return y\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& outer = *mod->functions[0];
  ASSERT_EQ(outer.body.size(), 3u);
  ASSERT_EQ(outer.body[1]->kind, ast::NodeKind::DefStmt);
  const auto* defstmt = static_cast<const ast::DefStmt*>(outer.body[1].get());
  ASSERT_TRUE(defstmt->func);
  const auto* inner = defstmt->func.get();
  ASSERT_EQ(inner->body.size(), 1u);
  ASSERT_EQ(inner->body[0]->kind, ast::NodeKind::ReturnStmt);
  const auto* ret = static_cast<const ast::ReturnStmt*>(inner->body[0].get());
  ASSERT_TRUE(ret->value);
  ASSERT_EQ(ret->value->kind, ast::NodeKind::Name);
  const auto* nm = static_cast<const ast::Name*>(ret->value.get());
  EXPECT_EQ(nm->id, std::string("y"));
}

