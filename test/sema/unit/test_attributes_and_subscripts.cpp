/***
 * Name: test_attributes_and_subscripts
 * Purpose: Typed subscript support for tuple/dict and attribute typing.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "attrs_subs.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaSubs, TupleIndexConstTyped) {
  const char* src = R"PY(
def f() -> int:
  t = (1, 'a')
  return t[0]
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaSubs, TupleIndexWrongReturnFails) {
  const char* src = R"PY(
def g() -> int:
  t = (1, 'a')
  return t[1]
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaSubs, TupleAliasIndexTyped) {
  const char* src = R"PY(
def h() -> int:
  t = (1, 2)
  u = t
  return u[1]
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaSubs, DictIndexTyped) {
  const char* src = R"PY(
def a() -> int:
  d = {'a': 1, 'b': 2}
  return d['a']
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaSubs, DictWrongKeyTypeFails) {
  const char* src = R"PY(
def b() -> int:
  d = {'a': 1}
  return d[0]
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaSubs, SetSubscriptRejected) {
  const char* src = R"PY(
def c() -> int:
  s = {1,2,3}
  return s[0]
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaAttr, AssignAndReadTyped) {
  const char* src = R"PY(
def d() -> int:
  obj = 0
  obj.x = 1
  return obj.x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaAttr, ReadUnknownAttrFailsReturn) {
  const char* src = R"PY(
def e() -> int:
  obj = 0
  return obj.x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

