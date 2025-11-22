/***
 * Name: test_sema_statistics_extras_typing
 * Purpose: Ensure Sema types statistics.stdev/pvariance and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK_statx(const char* src) {
  lex::Lexer L; L.pushString(src, "statx.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaStatisticsExtras, Accepts) {
  const char* src = R"PY(
def main() -> int:
  s = statistics.stdev([1,2,3])
  v = statistics.pvariance([1,2,3])
  return 0
)PY";
  EXPECT_TRUE(semaOK_statx(src));
}

TEST(SemaStatisticsExtras, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  s = statistics.stdev(123)
  return 0
)PY";
  EXPECT_FALSE(semaOK_statx(src1));
  const char* src2 = R"PY(
def main() -> int:
  s = statistics.pvariance('x')
  return 0
)PY";
  EXPECT_FALSE(semaOK_statx(src2));
}

