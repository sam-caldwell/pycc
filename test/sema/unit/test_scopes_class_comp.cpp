/***
 * Name: test_scopes_class_comp
 * Purpose: Ensure class and comprehension scopes do not leak names and are analyzed.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "class_comp.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaClassScope, NoLeakIntoOuter) {
  const char* src = R"PY(
def f() -> int:
  class C:
    x = 1
  return x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaClassScope, MethodDoesNotCaptureClassLocal) {
  const char* src = R"PY(
def f() -> int:
  class C:
    x = 1
    def m() -> int:
      return x
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaCompScope, TargetDoesNotLeak) {
  const char* src = R"PY(
def f() -> int:
  xs = [y for y in [1,2,3]]
  return y
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

