/***
 * Name: test_sema_math_typing
 * Purpose: Ensure Sema typing for math.* functions: arity and int/float acceptance.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src, const char* file="sema_math.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaMath, UnaryAcceptsIntFloatRejectsStr) {
  const char* ok = R"PY(
import math
def main() -> int:
  a = math.sqrt(4)
  b = math.floor(3.14)
  c = math.sin(1.0)
  d = math.log(2)
  return 0
)PY";
  EXPECT_TRUE(semaOK(ok));
  const char* bad = R"PY(
import math
def main() -> int:
  a = math.sqrt("x")
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad));
}

TEST(SemaMath, BinaryAcceptsIntFloatRejectsStr) {
  const char* ok = R"PY(
import math
def main() -> int:
  a = math.pow(2, 3)
  b = math.atan2(1.0, 1.0)
  c = math.fmod(5.0, 2.0)
  d = math.hypot(3.0, 4.0)
  e = math.copysign(1.0, -2.0)
  return 0
)PY";
  EXPECT_TRUE(semaOK(ok));
  const char* bad = R"PY(
import math
def main() -> int:
  a = math.pow("x", 2)
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad));
}

TEST(SemaMath, ArityMismatchRejected) {
  const char* bad = R"PY(
import math
def main() -> int:
  a = math.sqrt()
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad));
}

TEST(SemaMath, UnknownFunctionRejected) {
  const char* bad = R"PY(
import math
def main() -> int:
  a = math.not_a_func(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad));
}
