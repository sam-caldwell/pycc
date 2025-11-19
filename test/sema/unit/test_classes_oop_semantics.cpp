/***
 * Name: test_classes_oop_semantics
 * Purpose: Validate class/method semantics: __init__ return type, method binding, inherited methods.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "classes_oop.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ClassesOOP, ClassMethodCallOnClassNameOk) {
const char* src = R"PY(
class C:
  def m(a: int, b: int) -> int:
    return a
def main() -> int:
  return C.m(1, 2)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ClassesOOP, InitReturnMustBeNone) {
const char* src = R"PY(
class C:
  def __init__(self) -> int:
    return 0
def main() -> int:
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ClassesOOP, MethodKwOnlyAndDefaults) {
const char* src = R"PY(
class C:
  def m(a: int, b: int = 2, *, c: int) -> int:
    return a
def main() -> int:
  return C.m(5, c=3)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ClassesOOP, MethodMissingKwOnlyRejected) {
const char* src = R"PY(
class C:
  def m(a: int, b: int = 2, *, c: int) -> int:
    return a
def main() -> int:
  return C.m(5)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ClassesOOP, InheritedMethodCallOnDerived) {
const char* src = R"PY(
class B:
  def m(a: int) -> int:
    return a
class D(B):
  pass
def main() -> int:
  return D.m(1)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ClassesOOP, UnknownMethodRejected) {
const char* src = R"PY(
class C:
  pass
def main() -> int:
  return C.m(1)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ClassesOOP, InstanceMethodCallOnObjOk) {
const char* src = R"PY(
class C:
  def m(a: int) -> int:
    return a
def main() -> int:
  c = C()
  return c.m(5)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ClassesOOP, InstanceCallableViaDunderCall) {
const char* src = R"PY(
class F:
  def __call__(x: int, y: int) -> int:
    return x
def main() -> int:
  f = F()
  return f(2, 3)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ClassesOOP, MROLeftToRightOverrides) {
const char* src = R"PY(
class B:
  def m(a: int) -> int:
    return a
class E:
  def m(a: int, b: int) -> int:
    return a
class D(B, E):
  pass
def main() -> int:
  return D.m(1)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ClassesOOP, MROOrderMattersArityMismatch) {
const char* src = R"PY(
class B:
  def m(a: int) -> int:
    return a
class E:
  def m(a: int, b: int) -> int:
    return a
class D(E, B):
  pass
def main() -> int:
  return D.m(1)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ClassesOOP, DescriptorArityChecks) {
const char* src = R"PY(
class X:
  def __get__(a: int) -> int:
    return 0
def main() -> int:
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ClassesOOP, DunderLenMustReturnInt) {
const char* src = R"PY(
class X:
  def __len__(a: int) -> float:
    return 0.0
def main() -> int:
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ClassesOOP, GrandparentMethodResolved) {
const char* src = R"PY(
class A:
  def m(a: int) -> int:
    return a
class B(A):
  pass
class C(B):
  pass
def main() -> int:
  return C.m(7)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ClassesOOP, InstanceUnknownMethodRejected) {
const char* src = R"PY(
class C:
  pass
def main() -> int:
  c = C()
  return c.m(1)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ClassesOOP, GetAttrArityChecks) {
const char* src = R"PY(
class C:
  def __getattr__(x: int) -> int:
    return 0
def main() -> int:
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ClassesOOP, GetAttributeArityChecks) {
const char* src = R"PY(
class C:
  def __getattribute__(x: int, y: int, z: int) -> int:
    return 0
def main() -> int:
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ClassesOOP, SetAttrArityChecks) {
const char* src = R"PY(
class C:
  def __setattr__(a: int, b: int) -> int:
    return 0
def main() -> int:
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ClassesOOP, DelAttrArityChecks) {
const char* src = R"PY(
class C:
  def __delattr__(a: int, b: int, c: int) -> int:
    return 0
def main() -> int:
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ClassesOOP, DunderBoolAndStrReturns) {
const char* src = R"PY(
class C:
  def __bool__(a: int) -> int:
    return 0
class D:
  def __str__(a: int) -> int:
    return 0
class E:
  def __repr__(a: int) -> int:
    return 0
def main() -> int:
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ClassesOOP, DescriptorGetGoodArityAccepted) {
const char* src = R"PY(
class X:
  def __get__(a: int, b: int) -> int:
    return 0
def main() -> int:
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}
