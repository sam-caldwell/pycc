/***
 * Name: test_sema_abc_typing
 * Purpose: Validate typing/arity for _abc helpers.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src, const char* file="sema_abc.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaABC, AcceptsValidCalls) {
  const char* ok = R"PY(
import _abc
def main() -> int:
  t = _abc.get_cache_token()
  r = _abc.register("A", "B")
  q = _abc.is_registered("A", "B")
  _abc.invalidate_cache()
  _abc.reset()
  return 0
)PY";
  EXPECT_TRUE(semaOK(ok));
}

TEST(SemaABC, RejectsArityAndType) {
  const char* bad1 = R"PY(
import _abc
def main() -> int:
  _abc.get_cache_token(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad1));
  const char* bad2 = R"PY(
import _abc
def main() -> int:
  _abc.register(1, "B")
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad2));
}

