/***
 * Name: test_sema_colorsys_typing
 * Purpose: Ensure Sema types colorsys.rgb_to_hsv/hsv_to_rgb and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK_cs(const char* src) {
  lex::Lexer L; L.pushString(src, "cs.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaColorsys, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = colorsys.rgb_to_hsv(1.0, 0.0, 0.0)
  b = colorsys.hsv_to_rgb(0.0, 1.0, 1.0)
  return 0
)PY";
  EXPECT_TRUE(semaOK_cs(src));
}

TEST(SemaColorsys, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  a = colorsys.rgb_to_hsv('x', 0.0, 0.0)
  return 0
)PY";
  EXPECT_FALSE(semaOK_cs(src1));
}

