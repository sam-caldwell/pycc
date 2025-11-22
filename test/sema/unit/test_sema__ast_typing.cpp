/***
 * Name: test_sema__ast_typing
 * Purpose: Validate typing/arity for _ast helpers in this subset.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src, const char* file="sema__ast.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(Sema_Ast, AcceptsAndRejects) {
  const char* ok = R"PY(
import _ast
def main() -> int:
  s = _ast.dump("x")
  it = _ast.iter_fields("x")
  w = _ast.walk("x")
  c = _ast.copy_location("new", "old")
  f = _ast.fix_missing_locations("n")
  d = _ast.get_docstring("n")
  return 0
)PY";
  EXPECT_TRUE(semaOK(ok));
  const char* bad = R"PY(
import _ast
def main() -> int:
  s = _ast.dump(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad));
}

