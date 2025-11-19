/***
 * Name: test_exceptions_nested
 * Purpose: Nested try/except/finally and chained raises with extended exception mapping.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "exc_nested.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ExceptionsNested, NestedTryExceptFinallyWithRaiseFrom) {
  const char* src = R"PY(
def f() -> int:
  try:
    try:
      raise ValueError
    except ValueError as e:
      raise TypeError from e
    finally:
      t = 1
  except Exception:
    return 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ExceptionsNested, OSErrorShadowingDetected) {
  const char* src = R"PY(
def f() -> int:
  try:
    return 0
  except OSError:
    return 1
  except FileNotFoundError:
    return 2
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ExceptionsNested, SpecificThenOSErrorOk) {
  const char* src = R"PY(
def f() -> int:
  try:
    return 0
  except FileNotFoundError:
    return 1
  except OSError:
    return 2
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ExceptionsNested, BareRaiseRethrowInsideExceptOk) {
  const char* src = R"PY(
def f() -> int:
  try:
    try:
      raise ValueError
    except Exception:
      raise
  except Exception:
    return 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ExceptionsNested, FinallyDefinesNameDoesNotLeak) {
  const char* src = R"PY(
def f() -> int:
  try:
    raise ValueError
  finally:
    z = 1
  return z
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ExceptionsNested, NestedFinallyRaiseFromNoneOk) {
  const char* src = R"PY(
def f() -> int:
  try:
    try:
      raise FileNotFoundError
    finally:
      raise RuntimeError from None
  except RuntimeError:
    return 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ExceptionsNested, IOErrorAliasShadowingDetected) {
  const char* src = R"PY(
def f() -> int:
  try:
    return 0
  except IOError:
    return 1
  except FileNotFoundError:
    return 2
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}
