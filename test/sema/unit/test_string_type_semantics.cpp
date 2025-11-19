/***
 * Name: test_string_type_semantics
 * Purpose: Validate string typing for concatenation, comparisons, and membership.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "str_types.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaStr, ConcatOk) {
  const char* src = R"PY(
def f(x: str, y: str) -> str:
  return x + y
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaStr, ConcatMismatchFails) {
  const char* src = R"PY(
def f(x: str) -> str:
  return x + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaStr, EqCompareOk) {
  const char* src = R"PY(
def f(x: str, y: str) -> int:
  if x == y:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaStr, OrderingCompareFails) {
  const char* src = R"PY(
def f(x: str, y: str) -> int:
  if x < y:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaStr, MembershipInConditionOk) {
  const char* src = R"PY(
def f(x: str) -> int:
  if 'a' in x:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

