/***
 * Name: test_sema_string_typing
 * Purpose: Ensure Sema types string.capwords and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "strm.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaString, AcceptsCapwords) {
  const char* src = R"PY(
def main() -> int:
  a = string.capwords("hello world")
  b = string.capwords("a-b-c", "-")
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaString, RejectsBadArgs) {
  const char* src1 = R"PY(
def main() -> int:
  a = string.capwords(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = string.capwords("x", 1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

