/***
 * Name: test_sema_errno_typing
 * Purpose: Ensure Sema types errno.* functions and rejects arity.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "errno.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaErrno, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = errno.EPERM()
  b = errno.ENOENT()
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaErrno, RejectsArity) {
  const char* src = R"PY(
def main() -> int:
  a = errno.EPERM(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src));
}

