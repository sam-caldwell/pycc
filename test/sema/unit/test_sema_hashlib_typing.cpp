/***
 * Name: test_sema_hashlib_typing
 * Purpose: Ensure Sema types hashlib.sha256/md5 and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "hl.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaHashlib, AcceptsStrArg) {
  const char* src = R"PY(
def main() -> int:
  a = hashlib.sha256("hello")
  b = hashlib.md5("hello")
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaHashlib, RejectsWrongType) {
  const char* src = R"PY(
def main() -> int:
  a = hashlib.sha256(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src));
}

