/***
 * Name: test_constant_fold_strings
 * Purpose: Verify constant folding for string concatenation and comparisons.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/ConstantFold.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "cf_str.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ConstantFoldStrings, ConcatFold) {
  const char* src =
      "def main() -> int:\n"
      "  x = 'a' + 'b'\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  opt::ConstantFold cf; auto n = cf.run(*mod);
  EXPECT_GE(n, 1u);
  // Expect the concatenation to be folded into a single string literal in the assignment
  const auto& fn = *mod->functions[0];
  const auto* as = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_TRUE(as && as->value);
  ASSERT_EQ(as->value->kind, ast::NodeKind::StringLiteral);
  EXPECT_EQ(static_cast<const ast::StringLiteral*>(as->value.get())->value, std::string("ab"));
}

TEST(ConstantFoldStrings, CompareEqNeFold) {
  const char* src =
      "def main() -> int:\n"
      "  a = ('x' == 'x')\n"
      "  b = ('x' != 'y')\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  opt::ConstantFold cf; auto n = cf.run(*mod);
  EXPECT_GE(n, 2u);
  const auto& fn = *mod->functions[0];
  const auto* as0 = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  const auto* as1 = static_cast<const ast::AssignStmt*>(fn.body[1].get());
  ASSERT_TRUE(as0 && as0->value && as0->value->kind == ast::NodeKind::BoolLiteral);
  ASSERT_TRUE(as1 && as1->value && as1->value->kind == ast::NodeKind::BoolLiteral);
  EXPECT_TRUE(static_cast<const ast::BoolLiteral*>(as0->value.get())->value);
  EXPECT_TRUE(static_cast<const ast::BoolLiteral*>(as1->value.get())->value);
}

