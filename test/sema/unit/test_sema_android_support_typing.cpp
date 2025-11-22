/***
 * Name: test_sema_android_support_typing
 * Purpose: Validate typing/arity for _android_support helpers.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src, const char* file="sema_android_support.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaAndroidSupport, AcceptsValidCalls) {
  const char* ok = R"PY(
import _android_support
def main() -> int:
  a = _android_support.android_platform()
  b = _android_support.default_libdir()
  c = _android_support.ldflags()
  return 0
)PY";
  EXPECT_TRUE(semaOK(ok));
}

TEST(SemaAndroidSupport, RejectsArity) {
  const char* bad = R"PY(
import _android_support
def main() -> int:
  a = _android_support.android_platform(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad));
}

