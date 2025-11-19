/***
 * Name: test_simplify_cfg_canonicalizations
 * Purpose: Verify SimplifyCFG canonicalizes empty branches and simplifies elif chains.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/SimplifyCFG.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "cfg_canon.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SimplifyCFGCanon, EmptyThenSwappedByInversion) {
  const char* src = R"PY(
def main(a: bool) -> int:
  if a:
    pass
  else:
    return 7
)PY";
  auto mod = parseSrc(src);
  opt::SimplifyCFG cfg; auto n = cfg.run(*mod);
  EXPECT_GE(n, 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 1u);
  EXPECT_EQ(fn.body[0]->kind, ast::NodeKind::IfStmt);
}

TEST(SimplifyCFGCanon, EmptyIfRemoved) {
  const char* src = R"PY(
def main(a: bool) -> int:
  x = 1
  if a:
    pass
  else:
    pass
  return x
)PY";
  auto mod = parseSrc(src);
  opt::SimplifyCFG cfg; auto n = cfg.run(*mod);
  EXPECT_GE(n, 1u);
  const auto& fn = *mod->functions[0];
  // After removal, we should have just assignment + return (2 statements)
  ASSERT_EQ(fn.body.size(), 2u);
}

TEST(SimplifyCFGCanon, ElifChainWithTrueSimplifies) {
  const char* src = R"PY(
def main() -> int:
  if True:
    return 1
  elif False:
    return 2
  else:
    return 3
)PY";
  auto mod = parseSrc(src);
  opt::SimplifyCFG cfg; auto n = cfg.run(*mod);
  EXPECT_GE(n, 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 1u);
  EXPECT_EQ(fn.body[0]->kind, ast::NodeKind::ReturnStmt);
}

