/***
 * Name: test_sema_getpass_typing
 * Purpose: Ensure Sema types getpass.getuser/getpass and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "gp.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaGetpass, Accepts) {
  const char* src = R"PY(
def main() -> int:
  u = getpass.getuser()
  p = getpass.getpass("pwd:")
  q = getpass.getpass()
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaGetpass, RejectsArityOrType) {
  const char* src1 = R"PY(
def main() -> int:
  u = getpass.getuser(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  p = getpass.getpass(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

