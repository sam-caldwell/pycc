/***
 * Name: test_functions_calls_semantics
 * Purpose: Validate function signature binding: positional/keyword/defaults/kw-only, varargs and kwvarargs.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "func_calls.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(FuncCalls, PositionalOk) {
  const char* src = R"PY(
def f(a: int, b: int) -> int:
  return a
def g() -> int:
  return f(1, 2)
)PY";
  auto mod = parseSrc(src); sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(FuncCalls, MissingAndExtraArgsFail) {
  const char* src = R"PY(
def f(a: int, b: int) -> int:
  return a
def g1() -> int:
  return f(1)
def g2() -> int:
  return f(1,2,3)
)PY";
  auto mod = parseSrc(src); sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(FuncCalls, KeywordBindingOk) {
  const char* src = R"PY(
def f(a: int, b: int) -> int:
  return a
def g() -> int:
  return f(b=2, a=1)
)PY";
  auto mod = parseSrc(src); sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(FuncCalls, UnknownKeywordFails) {
  const char* src = R"PY(
def f(a: int) -> int:
  return a
def g() -> int:
  return f(c=1)
)PY";
  auto mod = parseSrc(src); sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(FuncCalls, DefaultsAndKwOnly) {
  const char* src = R"PY(
def f(a: int, b: int = 2, *, c: int) -> int:
  return a
def g1() -> int:
  return f(5, c=3)
def g2() -> int:
  return f(5)
)PY";
  auto mod = parseSrc(src); sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(FuncCalls, VarArgOkAndTypeChecked) {
  const char* src = R"PY(
def f(a: int, *args: int) -> int:
  return a
def g1() -> int:
  return f(1, 2, 3)
def g2() -> int:
  return f(1, 2.0)
)PY";
  auto mod = parseSrc(src); sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(FuncCalls, KwVarArgAcceptsUnknownKeywords) {
  const char* src = R"PY(
def f(**kw: int) -> int:
  return 0
def g() -> int:
  return f(x=1, y=2)
)PY";
  auto mod = parseSrc(src); sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(FuncCalls, StarArgsRequireVarArg) {
  const char* src = R"PY(
def f(a: int) -> int:
  return a
def g() -> int:
  xs = [1]
  return f(*xs)
)PY";
  auto mod = parseSrc(src); sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(FuncCalls, MultipleValuesForArgumentFails) {
  const char* src = R"PY(
def f(a: int) -> int:
  return a
def g() -> int:
  return f(1, a=2)
)PY";
  auto mod = parseSrc(src); sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}
