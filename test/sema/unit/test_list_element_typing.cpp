/***
 * Name: test_list_element_typing
 * Purpose: Type-check membership against list variables with known element type sets.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "list_elem.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaListElem, IntListAllowsIntMembership) {
  const char* src = R"PY(
def f() -> int:
  xs = [1,2,3]
  if 2 in xs:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaListElem, IntListRejectsStrMembership) {
  const char* src = R"PY(
def f() -> int:
  xs = [1,2,3]
  if 'a' in xs:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaListElem, AliasCopiesElemSet) {
  const char* src = R"PY(
def f() -> int:
  xs = [1,2]
  ys = xs
  if 1 in ys:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaListElem, UnionElementSet) {
  const char* src = R"PY(
def f() -> int:
  xs = [1, 'a']
  if 'a' in xs:
    return 1
  else:
    return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}
