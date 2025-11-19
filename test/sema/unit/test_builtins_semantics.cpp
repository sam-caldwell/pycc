/***
 * Name: test_builtins_semantics
 * Purpose: Semantic typing and arity checks for common built-ins in the subset.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "builtins_sem.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(BuiltinsSema, ConstructorsReturnTypes) {
  const char* src = R"PY(
def f(a: int, b: float, c: bool, d: str) -> int:
  i = int(b)
  f1 = float(a)
  bl = bool(0)
  s = str(a)
  lst = list((1,2))
  tp = tuple([1,2])
  dct = dict()
  rng = range(1,3)
  m = map(int, [1,2])
  p = print('x')
  return i
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(BuiltinsSema, SumInfersFromList) {
  const char* src = R"PY(
def f() -> int:
  x = sum([1,2,3])
  return x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(BuiltinsSema, ArityErrorsCaught) {
  const char* src = R"PY(
def f() -> int:
  x = float(1,2)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(BuiltinsSema, LenRejectsWrongArityAndType) {
  const char* src = R"PY(
def f() -> int:
  a = len(1)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(BuiltinsSema, MoreArityChecks) {
  const char* src = R"PY(
def f() -> int:
  a = range()
  b = range(1,2,3,4)
  c = isinstance(1)
  d = map(1)
  e = sum(1,2)
  f1 = bool(1,2)
  g = str(1,2)
  h = list(1,2)
  i = tuple(1,2)
  j = dict(1,2)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(BuiltinsSema, PrintReturnsNoneAndNotAddable) {
  const char* src = R"PY(
def f() -> int:
  x = print('x')
  return x + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(BuiltinsSema, RangeFormsAndSumFloat) {
  const char* src = R"PY(
def f() -> int:
  r1 = range(5)
  r2 = range(1, 5)
  r3 = range(1, 10, 2)
  s1 = sum([1.0, 2.0])
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(BuiltinsSema, IsInstanceWrongArityRejected) {
  const char* src = R"PY(
def f() -> int:
  a = isinstance(1)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(BuiltinsSema, LenDictLiteralOk) {
  const char* src = R"PY(
def f() -> int:
  return len({'a': 1, 'b': 2})
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}
