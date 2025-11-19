/***
 * Name: test_nested_logical_refinements
 * Purpose: Exercise nested combinations of and/or/not with None/isinstance.
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

TEST(NestedLogical, NotOverAndDistributesConservatively) {
  const char* src = R"PY(
def f(x: int) -> int:
  if not ((x != None) and isinstance(x, int)):
    # then branch: negation should hold; we don't rely on specific type here
    return 0
  else:
    return x + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

TEST(NestedLogical, OrOfAndsElseFailsUse) {
  const char* src = R"PY(
def f(x: int, y: int) -> int:
  if ((x != None) and isinstance(x, int)) or ((y != None) and isinstance(y, int)):
    return 0
  else:
    return x + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

