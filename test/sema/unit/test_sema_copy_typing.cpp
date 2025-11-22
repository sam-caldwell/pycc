/***
 * Name: test_sema_copy_typing
 * Purpose: Ensure Sema types copy.copy/deepcopy and rejects arity.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "cpy.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaCopy, Accepts) {
  const char* src = R"PY(
def main() -> int:
  import copy
  a = copy.copy([1,2,3])
  b = copy.deepcopy({"x": [1]})
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaCopy, RejectsArity) {
  const char* src = R"PY(
def main() -> int:
  import copy
  a = copy.copy()
  return 0
)PY";
  EXPECT_FALSE(semaOK(src));
}
