/***
 * Name: test_sema_bisect_insort_typing
 * Purpose: Ensure Sema types bisect alias/insort and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK_bi(const char* src) {
  lex::Lexer L; L.pushString(src, "bis2.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaBisect, AcceptsAliasAndInsort) {
  const char* src = R"PY(
def main() -> int:
  a = bisect.bisect([1,2,3], 2)
  bisect.insort_left([1,2,3], 2)
  bisect.insort_right([1,2,3], 2)
  bisect.insort([1,2,3], 2)
  return 0
)PY";
  EXPECT_TRUE(semaOK_bi(src));
}

TEST(SemaBisect, RejectsInsortBadArgs) {
  const char* src1 = R"PY(
def main() -> int:
  bisect.insort_left(1, 2)
  return 0
)PY";
  EXPECT_FALSE(semaOK_bi(src1));
  const char* src2 = R"PY(
def main() -> int:
  bisect.insort([1,2,3], "x")
  return 0
)PY";
  EXPECT_FALSE(semaOK_bi(src2));
}

