/***
 * Name: test_posonly_semantics
 * Purpose: Enforce positional-only parameters cannot be passed by keyword; allow valid positional usage.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "posonly.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(PosOnly, KeywordOnPosOnlyRejected) {
  const char* src =
      "def f(a: int, /, b: int) -> int:\n"
      "  return a\n"
      "def g() -> int:\n"
      "  return f(a=1, b=2)\n";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(PosOnly, PositionalAccepted) {
  const char* src =
      "def f(a: int, /, b: int) -> int:\n"
      "  return a\n"
      "def g() -> int:\n"
      "  return f(1, b=2)\n";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

