/***
 * Name: test_constant_fold_pow_float_zero
 * Purpose: Verify constant folding handles float pow with zero exponent.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/ConstantFold.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseCF(const char* src) {
  lex::Lexer L; L.pushString(src, "cf_pow0.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ConstantFold, FloatPowZeroExponent) {
  const char* src =
      "def main() -> float:\n"
      "  return 3.5 ** 0.0\n";
  auto mod = parseCF(src);
  opt::ConstantFold fold; const auto rewrites = fold.run(*mod); EXPECT_GE(rewrites, 1u);
  const auto& fn = *mod->functions[0]; const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
  ASSERT_EQ(ret->value->kind, ast::NodeKind::FloatLiteral);
  const auto* lit = static_cast<const ast::FloatLiteral*>(ret->value.get());
  EXPECT_DOUBLE_EQ(lit->value, 1.0);
}

