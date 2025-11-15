/***
 * Name: test_not_isinstance_refinement
 * Purpose: Ensure not isinstance() drives exclude/restrict and impacts downstream ops.
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

TEST(Sema, NotIsInstanceThenFailsIntOp) {
  const char* src = R"PY(
def f(x: int) -> int:
  if not isinstance(x, int):
    return x + 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(Sema, NotIsInstanceElseRestrictsOk) {
  const char* src = R"PY(
def f(x: int) -> int:
  if not isinstance(x, int):
    y = 0
  else:
    y = x
  return y + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

