/***
 * Name: test_merge_contradictions
 * Purpose: Ensure contradictions across branches are flagged on use (non-None unions too).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(MergeContradictions, IntElseStrUseInAddFails) {
  const char* src = R"PY(
def f(c: bool) -> int:
  if c:
    y = 1
  else:
    y = "a"
  return y + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(MergeContradictions, NestedAndOrElseFailsUse) {
  const char* src = R"PY(
def f(x: int, y: int) -> int:
  if ((x != None) and isinstance(x, int)) or ((y != None) and isinstance(y, int)):
    return 1
  else:
    return x + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

