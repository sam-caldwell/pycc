/***
 * Name: test_algebraic_id_minus_x
 * Purpose: Verify algebraic simplification reduces id(x) - x to 0 by default
 *          using interprocedural canonical propagation (no CLI flags).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"
#include "optimizer/AlgebraicSimplify.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(AlgebraicSimplify, IdMinusXBecomesZero) {
  const char* src =
      "def id(a: int) -> int:\n"
      "  if a == None:\n"
      "    return a\n"
      "  else:\n"
      "    return a\n"
      "def main() -> int:\n"
      "  x = 42\n"
      "  return id(x) - x\n";
  auto mod = parseSrc(src);

  // Run Sema to annotate types and canonical keys (calls adopt forwarded param canonical)
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  ASSERT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);

  // Inspect main: return should be an IntLiteral(0)
  ASSERT_EQ(mod->functions.size(), 2u);
  const auto& mainFn = *mod->functions[1];
  ASSERT_EQ(mainFn.name, "main");
  ASSERT_EQ(mainFn.body.size(), 2u);
  const auto* sRet = mainFn.body[1].get();
  ASSERT_EQ(sRet->kind, ast::NodeKind::ReturnStmt);
  const auto* ret = static_cast<const ast::ReturnStmt*>(sRet);
  ASSERT_TRUE(ret->value);
  ASSERT_EQ(ret->value->kind, ast::NodeKind::BinaryExpr);
  const auto* bin = static_cast<const ast::Binary*>(ret->value.get());
  // Canonicals equal (id(x) forwards x)
  ASSERT_TRUE(bin->lhs && bin->rhs);
  ASSERT_TRUE(bin->lhs->canonical().has_value());
  ASSERT_TRUE(bin->rhs->canonical().has_value());
  EXPECT_EQ(bin->lhs->canonical().value(), bin->rhs->canonical().value());
  // Run algebraic simplification directly (no flags), then expect literal 0
  opt::AlgebraicSimplify algebraic;
  const auto rewrites = algebraic.run(*mod);
  EXPECT_GE(rewrites, 1u);
  const auto* ret2 = static_cast<const ast::ReturnStmt*>(mainFn.body[1].get());
  ASSERT_TRUE(ret2->value);
  ASSERT_EQ(ret2->value->kind, ast::NodeKind::IntLiteral);
  const auto* lit0 = static_cast<const ast::IntLiteral*>(ret->value.get());
  EXPECT_EQ(lit0->value, 0);
}
