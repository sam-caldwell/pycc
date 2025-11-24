/***
 * Name: test_comprehension_shadowing
 * Purpose: Ensure comprehension loop targets shadow outer names without leaking.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* name = "comp_shadow.py") {
  lex::Lexer L; L.pushString(src, name);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaComprehensionShadow, ListCompShadowsOuterNameNoLeak) {
  const char* src = R"PY(
def f() -> int:
  y = 5
  xs = [y + 1 for y in [1,2,3]]
  return y
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaComprehensionShadow, NestedForsSeeEarlierTargetsButDoNotLeak) {
  const char* src = R"PY(
def f() -> int:
  x = 1
  xs = [x * y for x in [2,3] for y in [x,4]]
  return x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaComprehensionShadow, GeneratorExprShadowsOuterNameNoLeak) {
  const char* src = R"PY(
def f() -> int:
  y = 7
  z = sum(y for y in [1,2,3])
  return y
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

