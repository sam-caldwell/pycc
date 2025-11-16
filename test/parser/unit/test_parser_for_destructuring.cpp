/***
 * Name: test_parser_for_destructuring
 * Purpose: Verify for-loop destructuring targets (tuple/list nesting) parse and set Store ctx.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserForDestructuring, TupleListNested) {
  const char* src =
      "def main() -> int:\n"
      "  for a, (b, c) in arr:\n"
      "    pass\n"
      "  for [x, y] in arr:\n"
      "    pass\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "for_ds.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  ASSERT_TRUE(mod);
  const auto& fn = *mod->functions[0];
  ASSERT_GE(fn.body.size(), 3u);
  // First for
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::ForStmt);
  const auto* f1 = static_cast<const ast::ForStmt*>(fn.body[0].get());
  ASSERT_EQ(f1->target->kind, ast::NodeKind::TupleLiteral);
  const auto* tup = static_cast<const ast::TupleLiteral*>(f1->target.get());
  ASSERT_EQ(tup->elements.size(), 2u);
  ASSERT_EQ(tup->elements[0]->kind, ast::NodeKind::Name);
  ASSERT_EQ(tup->elements[1]->kind, ast::NodeKind::TupleLiteral);
  const auto* t1a = static_cast<const ast::Name*>(tup->elements[0].get());
  EXPECT_EQ(t1a->ctx, ast::ExprContext::Store);
  const auto* innerTup = static_cast<const ast::TupleLiteral*>(tup->elements[1].get());
  ASSERT_EQ(innerTup->elements.size(), 2u);
  const auto* t1b = static_cast<const ast::Name*>(innerTup->elements[0].get());
  const auto* t1c = static_cast<const ast::Name*>(innerTup->elements[1].get());
  EXPECT_EQ(t1b->ctx, ast::ExprContext::Store);
  EXPECT_EQ(t1c->ctx, ast::ExprContext::Store);
  // Second for
  ASSERT_EQ(fn.body[1]->kind, ast::NodeKind::ForStmt);
  const auto* f2 = static_cast<const ast::ForStmt*>(fn.body[1].get());
  ASSERT_EQ(f2->target->kind, ast::NodeKind::ListLiteral);
  const auto* lst = static_cast<const ast::ListLiteral*>(f2->target.get());
  ASSERT_EQ(lst->elements.size(), 2u);
  const auto* x = static_cast<const ast::Name*>(lst->elements[0].get());
  const auto* y = static_cast<const ast::Name*>(lst->elements[1].get());
  EXPECT_EQ(x->ctx, ast::ExprContext::Store);
  EXPECT_EQ(y->ctx, ast::ExprContext::Store);
}

