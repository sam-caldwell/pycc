/***
 * Name: test_sema_fnmatch_typing
 * Purpose: Ensure Sema types fnmatch.* and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "fm.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaFnmatch, AcceptsTypes) {
  const char* src = R"PY(
def main() -> int:
  a = fnmatch.fnmatch("ab", "a*")
  b = fnmatch.fnmatchcase("ab", "a?")
  c = fnmatch.translate("a*")
  d = fnmatch.filter(["a", "ab"], "a*")
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaFnmatch, RejectsWrongTypes) {
  const char* src1 = R"PY(
def main() -> int:
  a = fnmatch.fnmatch(1, "a*")
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = fnmatch.filter("notalist", "*")
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

