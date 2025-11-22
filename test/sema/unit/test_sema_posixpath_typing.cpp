/***
 * Name: test_sema_posixpath_typing
 * Purpose: Ensure Sema types posixpath subset and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK_pp(const char* src) {
  lex::Lexer L; L.pushString(src, "pp.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaPosixpath, Accepts) {
  const char* src = R"PY(
def main() -> int:
  j = posixpath.join('a', 'b')
  d = posixpath.dirname('/tmp/x')
  b = posixpath.basename('/tmp/x')
  s = posixpath.splitext('/tmp/x.txt')
  a = posixpath.abspath('x')
  e = posixpath.exists('/')
  return 0
)PY";
  EXPECT_TRUE(semaOK_pp(src));
}

TEST(SemaPosixpath, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  j = posixpath.join('a')
  return 0
)PY";
  EXPECT_FALSE(semaOK_pp(src1));
  const char* src2 = R"PY(
def main() -> int:
  e = posixpath.exists(123)
  return 0
)PY";
  EXPECT_FALSE(semaOK_pp(src2));
}

