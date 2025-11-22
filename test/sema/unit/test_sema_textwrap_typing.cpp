/***
 * Name: test_sema_textwrap_typing
 * Purpose: Ensure Sema types textwrap.fill/shorten and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "tw.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaTextwrap, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = textwrap.fill("This is a test", 6)
  b = textwrap.shorten("This is a test", 8)
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaTextwrap, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  a = textwrap.fill(1, 6)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = textwrap.shorten("x", "y")
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

