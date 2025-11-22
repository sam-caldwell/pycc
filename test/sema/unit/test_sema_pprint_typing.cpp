/***
 * Name: test_sema_pprint_typing
 * Purpose: Ensure Sema types pprint.pformat and rejects arity.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "pp.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaPprint, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = pprint.pformat([1,2,3])
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaPprint, RejectsArity) {
  const char* src = R"PY(
def main() -> int:
  a = pprint.pformat()
  return 0
)PY";
  EXPECT_FALSE(semaOK(src));
}

