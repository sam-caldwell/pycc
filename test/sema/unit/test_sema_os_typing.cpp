/***
 * Name: test_sema_os_typing
 * Purpose: Ensure Sema types os.* helpers and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "osmod.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaOS, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = os.getcwd()
  b = os.mkdir("dir")
  c = os.mkdir("dir2", 0o755)
  d = os.remove("file")
  e = os.rename("a", "b")
  f = os.getenv("PATH")
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaOS, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  a = os.getcwd(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = os.mkdir(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

