/***
 * Name: test_numeric_contradictions
 * Purpose: More checks for ambiguous numeric types across branches (int vs float).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "num_contra.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(NumericContradictions, BranchIntElseFloat_AddAmbiguous) {
  const char* src = R"PY(
def f(c: bool) -> int:
  if c:
    y = 1
  else:
    y = 1.0
  return y + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(NumericContradictions, BranchFloatElseInt_CompareAmbiguous) {
  const char* src = R"PY(
def f(c: bool) -> bool:
  if c:
    y = 1.0
  else:
    y = 1
  return y < 2
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

