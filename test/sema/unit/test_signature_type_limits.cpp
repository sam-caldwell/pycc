/***
 * Name: test_signature_type_limits
 * Purpose: Cover Sema restrictions on function return and parameter types.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "sig_limits.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SigLimits, ListReturnTypeRejected) {
  const char* src = R"PY(
def f() -> list:
  return []
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SigLimits, TupleParamTypeRejected) {
  const char* src = R"PY(
def f(x: tuple) -> int:
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

