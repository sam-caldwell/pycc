/***
 * Name: test_logical_operands_require_bool
 * Purpose: Ensure 'and'/'or' operands must be boolean-typed in Sema.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "logic_bool.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaLogic, AndRequiresBool) {
  const char* src = R"PY(
def f() -> int:
  if 1 and True:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaLogic, OrRequiresBool) {
  const char* src = R"PY(
def f() -> int:
  if False or 0:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

