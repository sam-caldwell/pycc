/***
 * Name: test_sema_eval_exec_typing
 * Purpose: Ensure eval/exec accept only literal strings and type is handled.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "eval_exec.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaEvalExec, AcceptLiteralStrings) {
  const char* src = R"PY(
def f() -> int:
  eval('1+2')
  exec('x=2')
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaEvalExec, RejectNonLiteral) {
  const char* src = R"PY(
def f() -> int:
  eval(1)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

