/***
 * Name: test_sema_linecache_typing
 * Purpose: Ensure Sema types linecache.getline and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "lc.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaLinecache, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = linecache.getline("file.txt", 2)
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaLinecache, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  a = linecache.getline(1, 2)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = linecache.getline("file.txt", "x")
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

