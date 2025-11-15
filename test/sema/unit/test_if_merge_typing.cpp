/***
 * Name: test_if_merge_typing
 * Purpose: Verify env merging after if intersects branch type sets.
 */
#include "sema/Sema.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include <gtest/gtest.h>

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(Sema, IfMerge_IntInt_AllowsAdd) {
  const char* src = R"PY(
def f(x: int) -> int:
  if x == None:
    y = 1
  else:
    y = 2
  return y + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

TEST(Sema, IfMerge_IntFloat_AmbiguousOrUndefinedFails) {
  const char* src = R"PY(
def f(x: int) -> int:
  if x == None:
    y = 1
  else:
    y = 2.0
  return y + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

