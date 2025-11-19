/***
 * Name: test_comprehensions_generators_semantics
 * Purpose: Semantic checks for list/dict/set comprehensions, generator expressions, and yield/yield from.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="compgen_sema.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CompGenSema, ListCompGuardMustBeBool) {
  const char* src = R"PY(
def main() -> int:
  a = [i for i in [1,2] if 3]
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(CompGenSema, DictCompGuardMustBeBool) {
  const char* src = R"PY(
def main() -> int:
  d = {k: v for (k, v) in [(1,2)] if 5}
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(CompGenSema, SetCompGuardBoolOk) {
  const char* src = R"PY(
def main() -> int:
  s = {x for x in [1,2] if x == 1}
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(CompGenSema, GeneratorGuardMustBeBool) {
  const char* src = R"PY(
def main() -> int:
  g = (i for i in [1,2] if 1)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(CompGenSema, YieldRejected) {
  const char* src = R"PY(
def main() -> int:
  x = yield 1
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(CompGenSema, YieldFromRejected) {
  const char* src = R"PY(
def main() -> int:
  it = [1,2]
  y = yield from it
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(CompGenSema, NestedListCompDestructureOk) {
  const char* src = R"PY(
def main() -> int:
  a = [i for j in [[1,2], [3]] for i in j if i == 1]
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(CompGenSema, DictCompTupleTargetOverNameIterOk) {
  const char* src = R"PY(
def main() -> int:
  xs = [(1,2),(3,4)]
  d = {k: v for (k, v) in xs if k == 1}
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(CompGenSema, GeneratorTupleTargetOk) {
  const char* src = R"PY(
def main() -> int:
  g = (k for (k, v) in [(1,2), (3,4)] if k)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(CompGenSema, SetCompGuardMustBeBool) {
  const char* src = R"PY(
def main() -> int:
  s = {x for x in [1,2] if 7}
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(CompGenSema, NestedTupleDestructureSetCompOk) {
  const char* src = R"PY(
def main() -> int:
  s = {(a,b) for (a,(b,c)) in [(1,(2,3)), (4,(5,6))] if b == 2}
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(CompGenSema, DictCompNestedTupleTargetOk) {
  const char* src = R"PY(
def main() -> int:
  d = {a: c for (a,(b,c)) in [(1,(2,3)), (4,(5,6))] if c == 3}
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(CompGenSema, ListCompMultiForMultiIfOk) {
  const char* src = R"PY(
def main() -> int:
  xs = [x for (x,y) in [(1,2),(3,4)] if x == 1 for (u,v) in [(5,6)] if u == 5]
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(CompGenSema, GeneratorNestedTupleTargetOk) {
  const char* src = R"PY(
def main() -> int:
  g = ((a,b) for (a,(b,c)) in [(1,(2,3))] if b == 2)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}
