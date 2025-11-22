/***
 * Name: test_sema_platform_typing
 * Purpose: Ensure Sema types platform.* and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "platm.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaPlatform, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = platform.system()
  b = platform.machine()
  c = platform.release()
  d = platform.version()
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaPlatform, RejectsArity) {
  const char* src = R"PY(
def main() -> int:
  a = platform.system(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src));
}

