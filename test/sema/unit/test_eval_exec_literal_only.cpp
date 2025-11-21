/***
 * Name: test_eval_exec_literal_only
 * Purpose: Sema accepts eval/exec only with literal string; rejects others.
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

TEST(SemaEvalExec, AcceptsLiteralOnly) {
  const char* src = R"PY(
def main() -> int:
  a = eval("123")
  b = exec("x=1")
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaEvalExec, RejectsNonLiteral) {
  const char* src = R"PY(
def main() -> int:
  s = "1+2"
  a = eval(s)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

