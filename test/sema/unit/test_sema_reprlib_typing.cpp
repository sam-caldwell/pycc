/***
 * Name: test_sema_reprlib_typing
 * Purpose: Ensure Sema types reprlib.repr and rejects invalid arity.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK_repr(const char* src) {
  lex::Lexer L; L.pushString(src, "rp.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaReprlib, Accepts) {
  const char* src = R"PY(
def main() -> int:
  s = reprlib.repr([1,2,3])
  return 0
)PY";
  EXPECT_TRUE(semaOK_repr(src));
}

TEST(SemaReprlib, Rejects) {
  const char* src = R"PY(
def main() -> int:
  s = reprlib.repr()
  return 0
)PY";
  EXPECT_FALSE(semaOK_repr(src));
}

