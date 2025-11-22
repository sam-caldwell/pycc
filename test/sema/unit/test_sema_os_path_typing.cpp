/***
 * Name: test_sema_os_path_typing
 * Purpose: Ensure Sema types os.path subset and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK_osp(const char* src) {
  lex::Lexer L; L.pushString(src, "osp.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaOsPath, Accepts) {
  const char* src = R"PY(
def main() -> int:
  j = os.path.join('a', 'b')
  d = os.path.dirname('/tmp/x')
  b = os.path.basename('/tmp/x')
  s = os.path.splitext('/tmp/x.txt')
  a = os.path.abspath('x')
  e = os.path.exists('/')
  return 0
)PY";
  EXPECT_TRUE(semaOK_osp(src));
}

TEST(SemaOsPath, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  j = os.path.join('a')
  return 0
)PY";
  EXPECT_FALSE(semaOK_osp(src1));
  const char* src2 = R"PY(
def main() -> int:
  e = os.path.exists(123)
  return 0
)PY";
  EXPECT_FALSE(semaOK_osp(src2));
}

