/***
 * Name: test_sema_glob_typing
 * Purpose: Ensure Sema types glob.* and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "globm.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaGlob, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = glob.glob("*.txt")
  b = glob.iglob("**/*.cpp")
  c = glob.escape("a*b?")
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaGlob, RejectsBadArgs) {
  const char* src1 = R"PY(
def main() -> int:
  a = glob.glob(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = glob.escape(2)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

