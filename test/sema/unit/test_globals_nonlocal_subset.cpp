/***
 * Name: test_globals_nonlocal_subset
 * Purpose: Ensure Sema recognizes 'global'/'nonlocal' declarations without treating them as local bindings.
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

TEST(SemaGlobalsNonlocal, GlobalAssignNoLocalBindingRequired) {
  const char* src = R"PY(
def f() -> int:
  global a
  a = 1
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

