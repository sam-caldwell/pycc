/***
 * Name: test_simplify_scopes
 * Purpose: Ensure scope simplifications (drop pass; merge identical return branches) preserve behavior.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/SimplifyScopes.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "scopes.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SimplifyScopes, DropsPass) {
  const char* src = R"PY(
def f() -> int:
  x = 1
  pass
  return x
)PY";
  auto mod = parseSrc(src);
  opt::SimplifyScopes simp; auto n = simp.run(*mod);
  EXPECT_GE(n, 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 2u);
  EXPECT_NE(fn.body[0]->kind, ast::NodeKind::PassStmt);
}

TEST(SimplifyScopes, MergeIdenticalReturnsInIf) {
  const char* src = R"PY(
def f(a: bool) -> int:
  if a:
    return 1
  else:
    return 1
)PY";
  auto mod = parseSrc(src);
  opt::SimplifyScopes simp; auto n = simp.run(*mod);
  EXPECT_GE(n, 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 1u);
  EXPECT_EQ(fn.body[0]->kind, ast::NodeKind::ReturnStmt);
}

