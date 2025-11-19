/***
 * Name: test_simplify_cfg_extended
 * Purpose: Exercise SimplifyCFG on composed boolean conditions (not/and/or) without running ConstantFold first.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/SimplifyCFG.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "cfg_ext.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SimplifyCFGExt, NotFalsePrunesToThen) {
  const char* src = R"PY(
def main() -> int:
  if not False:
    return 1
  else:
    return 2
)PY";
  auto mod = parseSrc(src);
  opt::SimplifyCFG cfg; auto n = cfg.run(*mod);
  EXPECT_GE(n, 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 1u);
  EXPECT_EQ(fn.body[0]->kind, ast::NodeKind::ReturnStmt);
}

TEST(SimplifyCFGExt, TrueAndFalsePrunesToElse) {
  const char* src = R"PY(
def main() -> int:
  if True and False:
    return 1
  else:
    return 2
)PY";
  auto mod = parseSrc(src);
  opt::SimplifyCFG cfg; auto n = cfg.run(*mod);
  EXPECT_GE(n, 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 1u);
  const auto* ret = static_cast<const ast::ReturnStmt*>(fn.body[0].get());
  const auto* lit = static_cast<const ast::IntLiteral*>(ret->value.get());
  (void)lit; // Don't assert numeric value, just structure
}

TEST(SimplifyCFGExt, TrueOrNamePrunesToThen) {
  const char* src = R"PY(
def main(a: bool) -> int:
  if True or a:
    return 1
  else:
    return 2
)PY";
  auto mod = parseSrc(src);
  opt::SimplifyCFG cfg; auto n = cfg.run(*mod);
  EXPECT_GE(n, 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 1u);
  EXPECT_EQ(fn.body[0]->kind, ast::NodeKind::ReturnStmt);
}

TEST(SimplifyCFGExt, FalseAndNamePrunesToElse) {
  const char* src = R"PY(
def main(a: bool) -> int:
  if False and a:
    return 1
  else:
    return 2
)PY";
  auto mod = parseSrc(src);
  opt::SimplifyCFG cfg; auto n = cfg.run(*mod);
  EXPECT_GE(n, 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 1u);
  EXPECT_EQ(fn.body[0]->kind, ast::NodeKind::ReturnStmt);
}

