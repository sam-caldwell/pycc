/***
 * Name: test_scope_conflicts
 * Purpose: Conflicting declarations in the same function and deeper global/nonlocal interactions.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* name = "scope_conflicts.py") {
  lex::Lexer L; L.pushString(src, name);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaScopeConflicts, GlobalAndNonlocalSameNameInSameFunctionFails) {
  const char* src = R"PY(
def outer() -> int:
  def inner() -> int:
    global x
    nonlocal x
    return 0
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaScopeConflicts, GlobalConflictsWithParameterNameFails) {
  const char* src = R"PY(
def f(x:int) -> int:
  global x
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaScopeConflicts, NonlocalConflictsWithInnerParamFails) {
  const char* src = R"PY(
def outer(y:int) -> int:
  def inner(y:int) -> int:
    nonlocal y
    return 0
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaScopeDeep, NonlocalSkipsGlobalDeclBindsOuterLocal) {
  const char* src = R"PY(
def a() -> int:
  x = 1
  def b() -> int:
    global x
    def c() -> int:
      nonlocal x
      x = x + 1
      return x
    return 0
  return x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaScopeConflicts, NonlocalInMethodCannotBindClassVar) {
  const char* src = R"PY(
def f() -> int:
  class C:
    x = 1
    def m() -> int:
      nonlocal x
      return 0
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

