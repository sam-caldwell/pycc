/***
 * Name: test_sema_bisect_typing
 * Purpose: Ensure Sema types bisect.* and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "bis.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaBisect, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = bisect.bisect_left([1,2,3], 2)
  b = bisect.bisect_right([1,2,3], 2)
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaBisect, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  a = bisect.bisect_left(1, 2)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = bisect.bisect_right([1,2,3], "x")
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

