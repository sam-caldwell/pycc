/***
 * Name: test_sema_shlex_typing
 * Purpose: Ensure Sema types shlex.split/join and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "shx.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaShlex, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = shlex.split("a 'b c'")
  b = shlex.join(["a", "b c"])
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaShlex, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  a = shlex.split(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  b = shlex.join("a")
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

