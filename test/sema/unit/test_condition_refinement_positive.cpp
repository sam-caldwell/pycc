/***
 * Name: test_condition_refinement_positive
 * Purpose: Positive cases for condition refinements: isinstance then-branch and not-eq-None patterns.
 */
#include "sema/Sema.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include <gtest/gtest.h>

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(Sema, IsInstanceThenBranchOk) {
  const char* src = R"PY(
def f(x: str) -> int:
  if isinstance(x, int):
    return x
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

TEST(Sema, NotEqNoneThenElseOk) {
  const char* src = R"PY(
def f(x: str) -> str:
  if not (x == None):
    return x
  else:
    return ""
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

TEST(Sema, OrElseNonNoneRefinedOk) {
  const char* src = R"PY(
def f(x: str, y: str) -> str:
  if (x == None) or (y == None):
    return ""
  else:
    return x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

TEST(Sema, AndThenRefineBNone) {
  const char* src = R"PY(
def f(a: bool, b: str) -> str:
  if a and (b == None):
    return ""
  else:
    return b
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

TEST(Sema, OrElseNestedNotNe) {
  const char* src = R"PY(
def f(x: str, y: str) -> str:
  if (x == None) or (not (y != None)):
    return ""
  else:
    return x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}
