/***
 * Name: test_scope_strict_read_before_assign
 * Purpose: Ensure Python3-like strict scoping: local read-before-assign fails; nonlocal can bind a parameter.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "scope_strict.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaScopeStrict, ReadBeforeAssignIsError) {
  const char* src = R"PY(
def f() -> int:
  x = x + 1
  return x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaScopeStrict, NonlocalMayBindParameter) {
  const char* src = R"PY(
def outer(a:int) -> int:
  def inner() -> int:
    nonlocal a
    a = a + 1
    return a
  return a
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

