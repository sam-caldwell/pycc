/***
 * Name: test_sema_statistics_typing
 * Purpose: Ensure Sema types statistics.mean/median and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "stats.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaStatistics, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = statistics.mean([1,2,3])
  b = statistics.median([1,2,3,4])
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaStatistics, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  a = statistics.mean(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = statistics.median(["x"])  # non-numeric tolerated as 0.0 in runtime, but typing forbids
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

