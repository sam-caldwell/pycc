/***
 * Name: test_constant_fold_bitwise
 * Purpose: Verify constant folding of bitwise and shift ops on int literals.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/ConstantFold.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "cf_bitwise.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ConstantFoldBitwise, AndOrXorShift) {
  const char* src =
      "def main() -> int:\n"
      "  return ((5 & 3) | (8 ^ 1)) << 1\n";
  auto mod = parseSrc(src);
  opt::ConstantFold fold;
  const auto rewrites = fold.run(*mod);
  EXPECT_GE(rewrites, 2u);
  const auto& fn = *mod->functions[0];
  const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
  ASSERT_TRUE(ret->value);
  ASSERT_EQ(ret->value->kind, ast::NodeKind::IntLiteral);
  const auto* lit = static_cast<const ast::IntLiteral*>(ret->value.get());
  // (5&3)=1, (8^1)=9, (1|9)=9, 9<<1 = 18
  EXPECT_EQ(lit->value, 18);
}

