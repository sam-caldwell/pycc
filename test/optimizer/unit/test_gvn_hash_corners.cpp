/***
 * Name: test_gvn_hash_corners
 * Purpose: Exercise GVN hashing on attributes, subscripts, tuples, and commutative shapes.
 */
#include <gtest/gtest.h>
#include "optimizer/GVN.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static opt::GVN::Result analyze(const char* src, const char* file="gvn2.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  opt::GVN gvn; return gvn.analyze(*mod);
}

TEST(GVNHash, AttributeOnStringLiteralIsPureAndClassified) {
  const char* src = R"PY(
def f() -> int:
  ("abc").upper
  ("abc").upper
  return 0
)PY";
  auto r = analyze(src);
  EXPECT_GE(r.expressions, 2u);
  EXPECT_LE(r.classes, r.expressions);
}

TEST(GVNHash, SubscriptOnTupleAndOnStringAreSeparated) {
  const char* src = R"PY(
def g() -> int:
  ("abcd")[1]
  (1,2,3)[1]
  return 0
)PY";
  auto r = analyze(src);
  EXPECT_EQ(r.expressions, 2u);
  EXPECT_EQ(r.classes, 2u);
}

TEST(GVNHash, CommutativeNotCanonicalized_YieldsDifferentClasses) {
  const char* src = R"PY(
def h() -> int:
  (1+2)
  (2+1)
  return 0
)PY";
  auto r = analyze(src);
  EXPECT_EQ(r.expressions, 2u);
  EXPECT_EQ(r.classes, 2u);
}

