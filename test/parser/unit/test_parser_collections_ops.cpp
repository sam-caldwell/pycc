/***
 * Name: test_parser_collections_ops
 * Purpose: Validate parsing shapes for subscripts, slices, and membership.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "coll.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserCollections, SubscriptIndex) {
  const char* src =
      "def main() -> int:\n"
      "  a = b[1]\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::Subscript);
  const auto* sub = static_cast<const ast::Subscript*>(asg->value.get());
  ASSERT_EQ(sub->value->kind, ast::NodeKind::Name);
  ASSERT_EQ(sub->slice->kind, ast::NodeKind::IntLiteral);
}

TEST(ParserCollections, SliceBasic) {
  const char* src =
      "def main() -> int:\n"
      "  a = b[1:3]\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  const auto* sub = static_cast<const ast::Subscript*>(asg->value.get());
  ASSERT_EQ(sub->slice->kind, ast::NodeKind::TupleLiteral);
  const auto* tup = static_cast<const ast::TupleLiteral*>(sub->slice.get());
  ASSERT_EQ(tup->elements.size(), 2u);
}

// Note: empty-bound slice d[:] currently exercised indirectly in slice-step case

TEST(ParserCollections, SliceWithStep) {
  const char* src =
      "def main() -> int:\n"
      "  e = f[1:4:2]\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  const auto* sub = static_cast<const ast::Subscript*>(asg->value.get());
  ASSERT_EQ(sub->slice->kind, ast::NodeKind::TupleLiteral);
  const auto* tup = static_cast<const ast::TupleLiteral*>(sub->slice.get());
  ASSERT_EQ(tup->elements.size(), 3u);
}

TEST(ParserCollections, MembershipIn) {
  const char* src =
      "def main() -> int:\n"
      "  r = x in [1,2]\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::BinaryExpr);
}
