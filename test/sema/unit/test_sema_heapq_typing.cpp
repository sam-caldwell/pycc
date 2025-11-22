/***
 * Name: test_sema_heapq_typing
 * Purpose: Ensure Sema types heapq.heappush/heappop and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "hpq.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaHeapq, Accepts) {
  const char* src = R"PY(
def main() -> int:
  import heapq
  a = [3,1,4]
  heapq.heappush(a, 2)
  x = heapq.heappop(a)
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaHeapq, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  import heapq
  heapq.heappush(1, 2)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  import heapq
  a = [1,2]
  x = heapq.heappop(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

