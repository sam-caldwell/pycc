/***
 * Name: test_imports_and_aliasing
 * Purpose: Ensure import and import-from statements bind names and do not error; allow use as attr bases.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="imports.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaImports, ImportAndFromBindNames) {
  const char* src = R"PY(
def f(x: int) -> int:
  return x
def main() -> int:
  import math
  from util import add as addalias
  math.add = f
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

