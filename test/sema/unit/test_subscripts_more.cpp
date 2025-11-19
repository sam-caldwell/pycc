/***
 * Name: test_subscripts_more
 * Purpose: Cover list/tuple subscript branches: list name index, non-int index error, and unknown index union path.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "subs_more.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SubscriptsMore, ListNameIndexTyped) {
  const char* src = R"PY(
def f() -> int:
  xs = [1,2,3]
  return xs[0]
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SubscriptsMore, TupleIndexNonIntFails) {
  const char* src = R"PY(
def g() -> int:
  t = (1,2)
  return t['a']
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SubscriptsMore, TupleUnknownIndexUnionPathAccepted) {
  const char* src = R"PY(
def h() -> int:
  t = (1,2)
  i = 1
  x = t[i]
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

