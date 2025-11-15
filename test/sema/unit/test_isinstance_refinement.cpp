/***
 * Name: test_isinstance_refinement
 * Purpose: Add coverage for positive isinstance() refinement and allow ops.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "sema_isinst.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaIsInstance, PositiveRefineIntAllowsAdd) {
  const char* src = R"PY(
def f(x: int) -> int:
  if isinstance(x, int):
    return x + 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

TEST(SemaIsInstance, PositiveRefineFloatAllowsAdd) {
  const char* src = R"PY(
def f(x: float) -> float:
  if isinstance(x, float):
    return x + 1.0
  else:
    return 0.0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

