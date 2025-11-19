/***
 * Name: test_constant_fold_extras
 * Purpose: Extra coverage for pow, floor-div, and boolean constant logic folding.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/ConstantFold.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "cf_extras.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ConstantFoldExtras, PowAndFloorDiv) {
  const char* src =
      "def main() -> int:\n"
      "  return (2 ** 3) // 3\n";
  auto mod = parseSrc(src);
  opt::ConstantFold fold;
  const auto rewrites = fold.run(*mod);
  EXPECT_GE(rewrites, 1u);
  const auto& fn = *mod->functions[0];
  const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
  ASSERT_TRUE(ret->value);
  ASSERT_EQ(ret->value->kind, ast::NodeKind::IntLiteral);
  const auto* lit = static_cast<const ast::IntLiteral*>(ret->value.get());
  EXPECT_EQ(lit->value, 2);
}

TEST(ConstantFoldExtras, BoolAndOrNot) {
  const char* src =
      "def main() -> bool:\n"
      "  return not (True and False) or False\n";
  auto mod = parseSrc(src);
  opt::ConstantFold fold;
  const auto rewrites = fold.run(*mod);
  EXPECT_GE(rewrites, 2u);
  const auto& fn = *mod->functions[0];
  const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
  ASSERT_TRUE(ret->value);
  ASSERT_EQ(ret->value->kind, ast::NodeKind::BoolLiteral);
  const auto* bl = static_cast<const ast::BoolLiteral*>(ret->value.get());
  EXPECT_TRUE(bl->value);
}
