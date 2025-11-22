/***
 * Name: test_sema_future_module
 * Purpose: Ensure __future__ import and attribute calls are semantically accepted/rejected as intended.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src, const char* file="sema_future.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaFuture, ImportFromAccepted) {
  const char* src = R"PY(
from __future__ import annotations
def main() -> int:
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaFuture, AttrCallZeroArgsOnly) {
  const char* ok = R"PY(
import __future__
def main() -> int:
  a = __future__.annotations()
  return 0
)PY";
  EXPECT_TRUE(semaOK(ok));
  const char* bad = R"PY(
import __future__
def main() -> int:
  a = __future__.annotations(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad));
}

TEST(SemaFuture, UnknownFeatureAcceptedZeroArgs) {
  const char* src = R"PY(
import __future__
def main() -> int:
  a = __future__.division()
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}
