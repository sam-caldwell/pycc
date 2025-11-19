/***
 * Name: test_is_isnot_none_refinement
 * Purpose: Ensure 'is None'/'is not None' refine branches similarly to eq/ne None.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "isnone.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaIsNone, ThenBranchSetsNone) {
  const char* src = R"PY(
def f(x: str) -> int:
  if x is None:
    return 0
  else:
    return len(x)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaIsNotNone, ElseBranchSetsNone) {
  const char* src = R"PY(
def f(x: str) -> int:
  if x is not None:
    return len(x)
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaNotIsNone, NegationRefines) {
  const char* src = R"PY(
def f(x: str) -> int:
  if not (x is None):
    return len(x)
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

