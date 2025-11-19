/***
 * Name: test_scope_enforcement
 * Purpose: Ensure Sema enforces scope rules for globals/nonlocals in the targeted subset.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "scope.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaScope, NonlocalWithoutEnclosingFails) {
  const char* src = R"PY(
def f() -> int:
  nonlocal a
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaScope, ReadGlobalWithoutDefinitionFails) {
  const char* src = R"PY(
def f() -> int:
  global a
  return a
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

