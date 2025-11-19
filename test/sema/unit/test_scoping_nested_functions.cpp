/***
 * Name: test_scoping_nested_functions
 * Purpose: Validate free var reads, nonlocal binds, and nested functions scoping.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "nested_scope.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaNestedScopes, FreeVarReadAllowed) {
  const char* src = R"PY(
def f() -> int:
  x = 1
  def g() -> int:
    return x + 1
  return x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaNestedScopes, NonlocalAssignUpdatesOuterType) {
  const char* src = R"PY(
def f() -> int:
  y = 1
  def g() -> int:
    nonlocal y
    y = y + 1
    return 0
  return y + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaNestedScopes, NonlocalNameNotFoundFails) {
  const char* src = R"PY(
def f() -> int:
  def g() -> int:
    nonlocal z
    return 0
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaNestedScopes, InnerAssignWithoutNonlocalIsLocal) {
  const char* src = R"PY(
def f() -> int:
  a = 1
  def g() -> int:
    a = 2
    return 0
  return a + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

