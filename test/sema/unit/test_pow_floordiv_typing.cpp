/***
 * Name: test_pow_floordiv_typing
 * Purpose: Ensure pow and floor-div typing for ints/floats and mismatches error.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrcPF(const char* src) {
  lex::Lexer L; L.pushString(src, "powfloor.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaPow, IntPowOk) {
  const char* src = R"PY(
def f(a: int, b: int) -> int:
  return a ** b
)PY";
  auto mod = parseSrcPF(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaPow, FloatPowOk) {
  const char* src = R"PY(
def f(a: float, b: float) -> float:
  return a ** b
)PY";
  auto mod = parseSrcPF(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaFloorDiv, IntFloorDivOk) {
  const char* src = R"PY(
def f(a: int, b: int) -> int:
  return a // b
)PY";
  auto mod = parseSrcPF(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaPowFloorDiv, MismatchAmbiguous) {
  const char* src = R"PY(
def f(a: int, b: float) -> int:
  return a ** b
)PY";
  auto mod = parseSrcPF(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

