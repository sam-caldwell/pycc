/***
 * Name: test_identity_membership_semantics
 * Purpose: Ensure identity and membership typing/enforcement across built-ins.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "id_mem.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaIdentity, IdentityAlwaysTypedBool) {
  const char* src = R"PY(
def f() -> int:
  if 1 is 1:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaMembership, StrInStrOk) {
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

TEST(SemaMembership, NonStrInStrFails) {
  const char* src = R"PY(
def f(x: str) -> int:
  if 1 in x:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaMembership, IntInListLiteralOk) {
  const char* src = R"PY(
def f() -> int:
  if 1 in [1,2,3]:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaMembership, RhsMustBeStrOrListFails) {
  const char* src = R"PY(
def f() -> int:
  if 1 in 2:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}
