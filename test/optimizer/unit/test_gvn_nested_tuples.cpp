/***
 * Name: test_gvn_nested_tuples
 * Purpose: Verify GVN hashing for nested tuple subscripts groups identical shapes.
 */
#include <gtest/gtest.h>
#include "optimizer/GVN.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static opt::GVN::Result analyze(const char* src, const char* file="gvn_nested.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  opt::GVN gvn; return gvn.analyze(*mod);
}

TEST(GVNNestedTuples, NestedTupleSubscriptClassStable) {
  const char* src = R"PY(
def f() -> int:
  (1,(2,3))[0]
  (1,(2,3))[0]
  return 0
)PY";
  auto r = analyze(src);
  // Two identical pure expressions collapse to one GVN class
  EXPECT_EQ(r.expressions, 2u);
  EXPECT_EQ(r.classes, 1u);
}

