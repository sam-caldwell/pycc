/***
 * Name: test_union_and_generics
 * Purpose: Cover union parameter acceptance and parametric list[T] typing in calls.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="union_gen.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaUnion, UnionParamAcceptsAlternatives) {
  const char* src = R"PY(
def f(x: int | str) -> int:
  return 0
def main() -> int:
  a = f(1)
  b = f("s")
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaUnion, OutOfUnionRejected) {
  const char* src = R"PY(
def f(x: int | str) -> int:
  return 0
def main() -> int:
  a = f(1.0)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaGenerics, ListElemTypeChecked) {
  const char* src = R"PY(
def g(xs: list[int]) -> int:
  return 0
def main() -> int:
  ok = g([1,2,3])
  bad = g([1.0])
  return 0
)PY";
  auto mod = parseSrc(src, "generics.py");
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

