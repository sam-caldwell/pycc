/***
 * Name: test_membership_typing
 * Purpose: Ensure 'in' and 'not in' type to bool and work in conditions.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "membership.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaMembership, InProducesBoolForIf) {
  const char* src = R"PY(
def f(x: int) -> int:
  if 1 in x:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaMembership, ReturnBoolRejectedBySignature) {
  const char* src = R"PY(
def f(x: int) -> int:
  return 1 in x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

