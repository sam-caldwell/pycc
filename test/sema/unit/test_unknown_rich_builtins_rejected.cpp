/***
 * Name: test_unknown_rich_builtins_rejected
 * Purpose: Validate that unmodeled rich builtins are rejected by Sema.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="rich_builtin.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaBuiltins, OpenNotModeledRejected) {
  const char* src = R"PY(
def main() -> int:
  f = open("/tmp/x")
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

