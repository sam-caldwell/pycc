/***
 * Name: test_unknown_function_call
 * Purpose: Cover error when calling an unknown function name.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "unknown_fn.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(UnknownFunctionCall, UnknownNameRejected) {
  const char* src = R"PY(
def main() -> int:
  return foo(1)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

