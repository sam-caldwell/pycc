/***
 * Name: test_sema_textwrap_extras_typing
 * Purpose: Ensure Sema types textwrap.wrap/dedent and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK_EX(const char* src) {
  lex::Lexer L; L.pushString(src, "tw_extras.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaTextwrapExtras, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = textwrap.wrap("This is a test", 6)
  b = textwrap.dedent("  This\n    is")
  return 0
)PY";
  EXPECT_TRUE(semaOK_EX(src));
}

TEST(SemaTextwrapExtras, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  a = textwrap.wrap(1, 6)
  return 0
)PY";
  EXPECT_FALSE(semaOK_EX(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = textwrap.dedent(123)
  return 0
)PY";
  EXPECT_FALSE(semaOK_EX(src2));
}

