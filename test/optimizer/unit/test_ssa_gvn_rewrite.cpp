/***
 * Name: test_ssa_gvn_rewrite
 * Purpose: Validate SSAGVN replaces repeated pure subexpressions using dominating assignment names.
 */
#include <gtest/gtest.h>
#include "optimizer/SSAGVN.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="ssagvn.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L); return P.parseModule();
}

TEST(SSAGVN, RewritesAcrossDominatedBlocks) {
  const char* src = R"PY(
def f() -> int:
  x = 1 + 2
  if 1:
    a = (1 + 2)
    b = (1 + 2)
  return 0
)PY";
  auto mod = parseSrc(src);
  opt::SSAGVN gvn; auto n = gvn.run(*mod);
  EXPECT_GE(n, 2u);
}

TEST(SSAGVN, DoesNotRewriteIfNameHasMultipleWrites) {
  const char* src = R"PY(
def g() -> int:
  x = 1 + 2
  x = 3
  if 1:
    a = (1 + 2)
  return 0
)PY";
  auto mod = parseSrc(src);
  opt::SSAGVN gvn; auto n = gvn.run(*mod);
  EXPECT_EQ(n, 0u);
}

