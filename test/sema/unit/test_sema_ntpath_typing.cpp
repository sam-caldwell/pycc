/***
 * Name: test_sema_ntpath_typing
 * Purpose: Ensure Sema types ntpath subset and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK_ntp(const char* src) {
  lex::Lexer L; L.pushString(src, "ntp.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaNtpath, Accepts) {
  const char* src = R"PY(
def main() -> int:
  j = ntpath.join('a', 'b')
  d = ntpath.dirname('C:/tmp/x')
  b = ntpath.basename('C:/tmp/x')
  s = ntpath.splitext('C:/tmp/x.txt')
  a = ntpath.abspath('x')
  e = ntpath.exists('/')
  return 0
)PY";
  EXPECT_TRUE(semaOK_ntp(src));
}

TEST(SemaNtpath, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  j = ntpath.join('a')
  return 0
)PY";
  EXPECT_FALSE(semaOK_ntp(src1));
  const char* src2 = R"PY(
def main() -> int:
  e = ntpath.exists(123)
  return 0
)PY";
  EXPECT_FALSE(semaOK_ntp(src2));
}

