/***
 * Name: test_multiple_var_merge
 * Purpose: Verify merges across multiple variables influence downstream operations.
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

TEST(Sema, MergeTwoVarsAllIntOk) {
  const char* src = R"PY(
def f(c: bool) -> int:
  if c:
    a = 1
    b = 2
  else:
    a = 3
    b = 4
  return a + b
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

TEST(Sema, MergePartialAmbiguityFailsOnUse) {
  const char* src = R"PY(
def f(c: bool) -> int:
  if c:
    a = 1
    b = 2.0
  else:
    a = 3
    b = 4
  return b + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

