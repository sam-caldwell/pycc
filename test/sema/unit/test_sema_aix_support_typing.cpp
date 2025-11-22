/***
 * Name: test_sema_aix_support_typing
 * Purpose: Validate typing/arity for _aix_support helpers.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src, const char* file="sema_aix_support.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaAIXSupport, AcceptsValidCalls) {
  const char* ok = R"PY(
import _aix_support
def main() -> int:
  a = _aix_support.aix_platform()
  b = _aix_support.default_libpath()
  c = _aix_support.ldflags()
  return 0
)PY";
  EXPECT_TRUE(semaOK(ok));
}

TEST(SemaAIXSupport, RejectsArity) {
  const char* bad = R"PY(
import _aix_support
def main() -> int:
  a = _aix_support.aix_platform(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad));
}

