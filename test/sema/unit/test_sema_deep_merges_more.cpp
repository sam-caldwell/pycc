/***
 * Name: test_sema_deep_merges_more
 * Purpose: Additional deep-nesting merge tests for condition refinements.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "deep_merge.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaDeepMerges, AndWithNotIsInstanceThenFailsAdd) {
  const char* src = R"PY(
def f(x: int) -> int:
  if (not isinstance(x, int)) and (x != None or isinstance(x, float)):
    return x + 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaDeepMerges, ElifChainMergeContradictions_FailUse) {
  const char* src = R"PY(
def g(x: str) -> int:
  if x == None:
    pass
  elif isinstance(x, int):
    pass
  else:
    pass
  # After merging contradictory branches, x is not guaranteed str
  return len(x)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}
