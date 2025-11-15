/***
 * Name: test_complex_nested_mixture
 * Purpose: Validate nested And/Or with isinstance and None across two variables.
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

TEST(Sema, ComplexNestedMixtureAmbiguous) {
  const char* src = R"PY(
def f(x: int, y: int) -> int:
  if (isinstance(x, int) and (y != None)) or (not isinstance(y, int)):
    u = x
    v = 2.0
  else:
    u = x
    v = 3
  return u + v
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

