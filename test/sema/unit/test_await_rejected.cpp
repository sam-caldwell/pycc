/***
 * Name: test_await_rejected
 * Purpose: Ensure 'await' is rejected by Sema in this subset.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "await_sem.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaAwait, AwaitRejected) {
  const char* src =
      "def main() -> int:\n"
      "  x = await 1\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

