/***
 * Name: test_parser_attribute_subscript
 * Purpose: Ensure attribute and subscript (index/slice/multi-index) parse and chain.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/Attribute.h"
#include "ast/Subscript.h"
#include "ast/TupleLiteral.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "attrsub.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserAttrSub, ChainedAttrSubscriptCall) {
  const char* src =
      "def main() -> int:\n"
      "  x = a.b.c(1)[2:3].d\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  // Expect final attribute
  ASSERT_EQ(asg->value->kind, ast::NodeKind::Attribute);
  const auto* attr = static_cast<const ast::Attribute*>(asg->value.get());
  EXPECT_EQ(attr->attr, std::string("d"));
}

TEST(ParserAttrSub, MultiIndexTuple) {
  const char* src =
      "def main() -> int:\n"
      "  y = arr[1, 2, 3]\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::Subscript);
  const auto* sub = static_cast<const ast::Subscript*>(asg->value.get());
  ASSERT_EQ(sub->slice->kind, ast::NodeKind::TupleLiteral);
  const auto* tup = static_cast<const ast::TupleLiteral*>(sub->slice.get());
  ASSERT_EQ(tup->elements.size(), 3u);
}

TEST(ParserAttrSub, SliceEmptyBounds) {
  const char* src =
      "def main() -> int:\n"
      "  z = d[:]\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::Subscript);
  const auto* sub = static_cast<const ast::Subscript*>(asg->value.get());
  ASSERT_EQ(sub->slice->kind, ast::NodeKind::TupleLiteral);
  const auto* tup = static_cast<const ast::TupleLiteral*>(sub->slice.get());
  ASSERT_EQ(tup->elements.size(), 2u);
}

