/***
 * Name: test_membership_list_name_edges
 * Purpose: Cover membership when RHS is a named list with known element set.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "mem_list_name.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(MembershipListName, IntInNamedListOk) {
  const char* src = R"PY(
def f() -> int:
  xs = [1,2,3]
  if 1 in xs:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(MembershipListName, StrInNamedIntListRejected) {
  const char* src = R"PY(
def f() -> int:
  xs = [1,2,3]
  if 'a' in xs:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

