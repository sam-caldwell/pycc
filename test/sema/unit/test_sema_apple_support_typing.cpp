/***
 * Name: test_sema_apple_support_typing
 * Purpose: Validate typing/arity for _apple_support helpers.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src, const char* file="sema_apple_support.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaAppleSupport, AcceptsValidCalls) {
  const char* ok = R"PY(
import _apple_support
def main() -> int:
  a = _apple_support.apple_platform()
  b = _apple_support.default_sdkroot()
  c = _apple_support.ldflags()
  return 0
)PY";
  EXPECT_TRUE(semaOK(ok));
}

TEST(SemaAppleSupport, RejectsArity) {
  const char* bad = R"PY(
import _apple_support
def main() -> int:
  a = _apple_support.apple_platform(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad));
}

