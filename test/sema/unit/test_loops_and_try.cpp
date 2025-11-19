/***
 * Name: test_loops_and_try
 * Purpose: Ensure loops do not leak inner-only bindings; try/except/else merges are conservative.
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

TEST(Loops, WhileInnerBindingUndefinedAfter) {
  const char* src = R"PY(
def f(n: int) -> int:
  while n:
    z = 1
    break
  return z
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(TryExcept, IntElseStrUsedInAddFails) {
  const char* src = R"PY(
def f(c: bool) -> int:
  try:
    if c:
      y = 1
    else:
      y = 1
  except Exception as e:
    y = "a"
  return y + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

