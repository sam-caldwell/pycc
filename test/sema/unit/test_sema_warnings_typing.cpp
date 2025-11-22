/***
 * Name: test_sema_warnings_typing
 * Purpose: Ensure Sema types warnings.warn/simplefilter and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "warn.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaWarnings, Accepts) {
  const char* src = R"PY(
def main() -> int:
  warnings.warn("oops")
  warnings.simplefilter("ignore")
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaWarnings, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  warnings.warn(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  warnings.simplefilter(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

