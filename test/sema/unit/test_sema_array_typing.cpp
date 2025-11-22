/***
 * Name: test_sema_array_typing
 * Purpose: Ensure Sema types array subset and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK_array(const char* src) {
  lex::Lexer L; L.pushString(src, "arr.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaArray, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = array.array('i', [1,2,3])
  array.append(a, 4)
  x = array.pop(a)
  b = array.tolist(a)
  return 0
)PY";
  EXPECT_TRUE(semaOK_array(src));
}

TEST(SemaArray, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  a = array.array(1, [1,2])
  return 0
)PY";
  EXPECT_FALSE(semaOK_array(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = array.array('i', 123)
  return 0
)PY";
  EXPECT_FALSE(semaOK_array(src2));
  const char* src3 = R"PY(
def main() -> int:
  array.append([], 'x')
  return 0
)PY";
  EXPECT_FALSE(semaOK_array(src3));
}

