/***
 * Name: test_parser_comparisons_all
 * Purpose: Verify all comparison operators parse correctly; single and chained.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/Binary.h"
#include "ast/Compare.h"
#include "ast/ListLiteral.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "cmp_all.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserCompareOps, Singles) {
  const char* src =
      "def f() -> int:\n"
      "  a = 1 == 1\n"
      "  b = 1 != 2\n"
      "  c = 1 < 2\n"
      "  d = 1 <= 2\n"
      "  e = 2 > 1\n"
      "  f = 2 >= 1\n"
      "  g = x is y\n"
      "  h = x is not y\n"
      "  i = x in [1,2]\n"
      "  j = x not in [1,2]\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  auto checkBin = [&](int idx, ast::BinaryOperator op) {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[idx].get());
    ASSERT_EQ(asg->value->kind, ast::NodeKind::BinaryExpr);
    const auto* bin = static_cast<const ast::Binary*>(asg->value.get());
    EXPECT_EQ(bin->op, op);
  };
  checkBin(0, ast::BinaryOperator::Eq);
  checkBin(1, ast::BinaryOperator::Ne);
  checkBin(2, ast::BinaryOperator::Lt);
  checkBin(3, ast::BinaryOperator::Le);
  checkBin(4, ast::BinaryOperator::Gt);
  checkBin(5, ast::BinaryOperator::Ge);
  checkBin(6, ast::BinaryOperator::Is);
  checkBin(7, ast::BinaryOperator::IsNot);
  checkBin(8, ast::BinaryOperator::In);
  checkBin(9, ast::BinaryOperator::NotIn);
}

TEST(ParserCompareOps, ChainedAndMixed) {
  const char* src =
      "def f() -> int:\n"
      "  k = 1 < 2 < 3\n"
      "  l = 1 == 2 < 3\n"
      "  m = x is y is not z\n"
      "  n = a in [1] != b\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  auto checkCmp = [&](int idx, std::initializer_list<ast::BinaryOperator> ops) {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[idx].get());
    ASSERT_EQ(asg->value->kind, ast::NodeKind::Compare);
    const auto* cmp = static_cast<const ast::Compare*>(asg->value.get());
    ASSERT_EQ(cmp->ops.size(), ops.size());
    size_t i = 0; for (auto expected : ops) { EXPECT_EQ(cmp->ops[i++], expected); }
  };
  checkCmp(0, {ast::BinaryOperator::Lt, ast::BinaryOperator::Lt});
  checkCmp(1, {ast::BinaryOperator::Eq, ast::BinaryOperator::Lt});
  checkCmp(2, {ast::BinaryOperator::Is, ast::BinaryOperator::IsNot});
  checkCmp(3, {ast::BinaryOperator::In, ast::BinaryOperator::Ne});
}

