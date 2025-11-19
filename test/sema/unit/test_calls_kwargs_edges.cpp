/***
 * Name: test_calls_kwargs_edges
 * Purpose: Cover **kwargs provided without kwvarargs in callee.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "kwargs_edges.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CallKwargsEdges, KwStarArgsWithoutKwVarargsRejected) {
  const char* src = R"PY(
def f(a: int) -> int:
  return a
def g() -> int:
  d = {'a': 1}
  return f(**d)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

