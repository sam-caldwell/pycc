/***
 * Name: test_sema_keyword_typing
 * Purpose: Ensure Sema types keyword.iskeyword/kwlist and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "kw.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaKeyword, AcceptsTypes) {
  const char* src = R"PY(
def main() -> int:
  a = keyword.iskeyword("for")
  b = keyword.kwlist()
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaKeyword, RejectsWrongArityAndType) {
  const char* src1 = R"PY(
def main() -> int:
  a = keyword.iskeyword(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = keyword.kwlist(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

