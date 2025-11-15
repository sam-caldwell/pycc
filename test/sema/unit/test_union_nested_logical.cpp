/***
 * Name: test_union_nested_logical
 * Purpose: Combine isinstance and None nested with or/and; verify merges on multiple vars.
 */
#include "sema/Sema.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include <gtest/gtest.h>

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(Sema, NestedIsInstanceOrNoneElseAmbiguous) {
  const char* src = R"PY(
def f(x: int, y: int) -> int:
  if isinstance(x, int) or (y == None):
    z = 1
  else:
    z = 2.0
  return z + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(Sema, NestedAndThenElseBothInt) {
  const char* src = R"PY(
def f(a: bool, b: int) -> int:
  if a and (b != None):
    z = b
  else:
    z = 0
  return z + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

