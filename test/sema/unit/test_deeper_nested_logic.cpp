/***
 * Name: test_deeper_nested_logic
 * Purpose: Deeper nested not/or/and refinements with isinstance/None.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "deep_logic.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(DeeperNestedLogic, NotOverOrExcludesIntAndNone_ThenFailsAdd) {
  const char* src = R"PY(
def f(x: int) -> int:
  if not (isinstance(x, int) or (x == None)):
    return x + 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(DeeperNestedLogic, AndOfDoubleNotIsInstance_ThenFailsAdd) {
  const char* src = R"PY(
def f(x: int) -> int:
  if (not isinstance(x, int)) and (not isinstance(x, float)):
    return x + 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

