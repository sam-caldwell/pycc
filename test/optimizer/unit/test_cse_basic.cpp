/***
 * Name: test_cse_basic
 * Purpose: Cover CSE duplicate pure expr-stmt removal and intra-statement subexpr rewriting.
 */
#include <gtest/gtest.h>
#include "optimizer/CSE.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="cse.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L); return P.parseModule();
}

TEST(CSE, RemovesDuplicatePureExprStmts) {
  const char* src = R"PY(
def f() -> int:
  1 + 2
  1 + 2
  return 0
)PY";
  auto mod = parseSrc(src);
  opt::CSE cse; auto n = cse.run(*mod);
  EXPECT_GE(n, 1u);
}

TEST(CSE, RewritesSubexprWithTemp) {
  const char* src = R"PY(
def g() -> int:
  y = (1 + 2) + (1 + 2)
  return 0
)PY";
  auto mod = parseSrc(src);
  opt::CSE cse; auto n = cse.run(*mod);
  EXPECT_GE(n, 1u);
}

