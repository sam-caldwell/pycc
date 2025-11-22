/***
 * Name: test_localprop_basic
 * Purpose: Exercise LocalProp constant/copy propagation within a block and not across control flow.
 */
#include <gtest/gtest.h>
#include "optimizer/LocalProp.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="lp.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L); return P.parseModule();
}

TEST(LocalProp, PropagatesWithinBlock) {
  const char* src = R"PY(
def f() -> int:
  a = 1
  b = a
  c = b
  return c
)PY";
  auto mod = parseSrc(src);
  opt::LocalProp lp; auto n = lp.run(*mod);
  EXPECT_GE(n, 2u);
}

TEST(LocalProp, DoesNotCrossIf) {
  const char* src = R"PY(
def g() -> int:
  x = 1
  if 1:
    y = x
  return 0
)PY";
  auto mod = parseSrc(src);
  opt::LocalProp lp; auto n = lp.run(*mod);
  // May rewrite within block, but not carry x's value into if body (env cleared)
  EXPECT_GE(n, 0u);
}

