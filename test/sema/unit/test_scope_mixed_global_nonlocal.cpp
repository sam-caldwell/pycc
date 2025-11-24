/***
 * Name: test_scope_mixed_global_nonlocal
 * Purpose: More edge cases in mixed global/nonlocal usage across multiple levels.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* name = "mixed_scope.py") {
  lex::Lexer L; L.pushString(src, name);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaMixedScopes, GlobalInsideInnerOverridesOuterLocal) {
  const char* src = R"PY(
def outer() -> int:
  a = 1
  def inner() -> int:
    global a
    a = 2
    return 0
  return a
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  // Allow global in inner even if outer has a local of same name.
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaMixedScopes, NonlocalBindsAcrossTwoLevels) {
  const char* src = R"PY(
def f() -> int:
  a = 1
  def g() -> int:
    def h() -> int:
      nonlocal a
      a = a + 1
      return a
    return 0
  return a
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaMixedScopes, NonlocalCannotBindGlobal) {
  const char* src = R"PY(
def outer() -> int:
  global x
  def inner() -> int:
    nonlocal x
    return 0
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaMixedScopes, NonlocalBindsNearestEvenIfParamShadowed) {
  const char* src = R"PY(
def f(a:int) -> int:
  def g() -> int:
    a = 2
    def h() -> int:
      nonlocal a
      a = a + 1
      return a
    return 0
  return a
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

