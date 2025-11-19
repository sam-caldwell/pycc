/***
 * Name: test_names_and_flow_full
 * Purpose: Drive Names/typing basics and control-flow refinements to 100% for None/isinstance/not/and/or.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(NamesBasics, UndefinedNameInExpressionFails) {
  const char* src = R"PY(
def main() -> int:
  return x + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(NamesBasics, DefinedOnlyInThenUndefinedAfterIf) {
  const char* src = R"PY(
def main() -> int:
  if 1 == 1:
    y = 3
  else:
    pass
  return y
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(NamesBasics, ContradictoryMergeFlagsOnUse) {
  const char* src = R"PY(
def f(x: str) -> int:
  if x == None:
    pass
  else:
    pass
  return len(x)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(FlowRefine, AndRefinesBothInThen) {
  const char* src = R"PY(
def f(x: int) -> int:
  if (x != None) and isinstance(x, int):
    return x + 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

TEST(FlowRefine, OrElseNegationApplied) {
  const char* src = R"PY(
def f(x: str, y: int) -> int:
  if (x == None) or isinstance(y, int):
    return 0
  else:
    # else branch negates both: x != None and not isinstance(y, int)
    return y  # should fail: y no longer int in else
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(FlowRefine, NotOverEqNoneRefinesThen) {
  const char* src = R"PY(
def f(x: str) -> int:
  if not (x == None):
    return len(x)
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

TEST(FlowRefine, NotOverNeNoneRefinesElse) {
  const char* src = R"PY(
def f(x: str) -> int:
  if not (x != None):
    # x == None here
    return 0
  else:
    return len(x)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

