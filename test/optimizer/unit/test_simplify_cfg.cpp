/***
 * Name: test_simplify_cfg
 * Purpose: Verify SimplifyCFG prunes constant if-statements.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/SimplifyCFG.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "cfg.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SimplifyCFG, IfTruePrunesToThen) {
  const char* src = R"PY(
def main() -> int:
  if True:
    return 1
  else:
    return 2
)PY";
  auto mod = parseSrc(src);
  opt::SimplifyCFG cfg;
  auto n = cfg.run(*mod);
  EXPECT_GE(n, 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 1u);
  EXPECT_EQ(fn.body[0]->kind, ast::NodeKind::ReturnStmt);
}

TEST(SimplifyCFG, IfFalsePrunesToElse) {
  const char* src = R"PY(
def main() -> int:
  if False:
    return 1
  else:
    return 2
)PY";
  auto mod = parseSrc(src);
  opt::SimplifyCFG cfg;
  auto n = cfg.run(*mod);
  EXPECT_GE(n, 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 1u);
  EXPECT_EQ(fn.body[0]->kind, ast::NodeKind::ReturnStmt);
}

