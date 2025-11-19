/***
 * Name: test_exceptions_semantics
 * Purpose: Exceptions semantics to 100%: raise/except matching, chaining, context, hierarchy & shadowing.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "exc.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(Exceptions, RaiseNonExceptionFails) {
  const char* src = R"PY(
def f() -> int:
  raise 1
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(Exceptions, RaiseFromNonExceptionFails) {
  const char* src = R"PY(
def f() -> int:
  raise ValueError from 1
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(Exceptions, BareRaiseOutsideExceptFails) {
  const char* src = R"PY(
def f() -> int:
  raise
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(Exceptions, ExceptTypeValidationAndShadowing) {
  const char* src = R"PY(
def f() -> int:
  try:
    return 0
  except (ValueError, TypeError):
    return 1
  except Exception:
    return 2
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(Exceptions, ExceptInvalidTypeRejected) {
  const char* src = R"PY(
def f() -> int:
  try:
    return 0
  except (1, ValueError):
    return 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(Exceptions, ShadowedSpecificAfterGeneralRejected) {
  const char* src = R"PY(
def f() -> int:
  try:
    return 0
  except Exception:
    return 1
  except ValueError:
    return 2
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

