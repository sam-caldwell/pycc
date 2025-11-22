/***
 * Name: test_sema_types_typing
 * Purpose: Ensure Sema types types.SimpleNamespace and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK_types(const char* src) {
  lex::Lexer L; L.pushString(src, "types_ns.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaTypes, Accepts) {
  const char* src = R"PY(
def main() -> int:
  ns = types.SimpleNamespace()
  ns2 = types.SimpleNamespace([['a', 1], ['b', 'x']])
  return 0
)PY";
  EXPECT_TRUE(semaOK_types(src));
}

TEST(SemaTypes, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  ns = types.SimpleNamespace(123)
  return 0
)PY";
  EXPECT_FALSE(semaOK_types(src1));
}

