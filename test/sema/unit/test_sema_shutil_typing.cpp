/***
 * Name: test_sema_shutil_typing
 * Purpose: Ensure Sema types shutil.copyfile/copy and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "shumod.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaShutil, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = shutil.copyfile("a", "b")
  b = shutil.copy("b", "c")
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaShutil, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  a = shutil.copyfile(1, "b")
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = shutil.copy("b")
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

