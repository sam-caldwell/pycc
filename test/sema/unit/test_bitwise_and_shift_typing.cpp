/***
 * Name: test_bitwise_and_shift_typing
 * Purpose: Ensure bitwise and shift ops are int-only and typed int.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "bitwise.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaBitwise, IntBitwiseOk) {
  const char* src = R"PY(
def f(a: int, b: int) -> int:
  return (a & b) | (a ^ b)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

TEST(SemaShift, ShiftRequiresInt) {
  const char* src = R"PY(
def f(a: float, b: int) -> int:
  return a << b
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaUnaryBitNot, RequiresInt) {
  const char* src = R"PY(
def f(a: float) -> int:
  return ~a
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

