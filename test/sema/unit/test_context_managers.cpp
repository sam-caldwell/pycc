/***
 * Name: test_context_managers
 * Purpose: Context managers semantics: with/async with; multiple items; as-binding; block scope behavior.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "with_semantics.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ContextManagers, AsBindingVisibleAfter) {
  const char* src = R"PY(
def f() -> int:
  v = 5
  with v as x:
    pass
  return x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ContextManagers, MultipleItemsBindAndUse) {
  const char* src = R"PY(
def f() -> int:
  a = 1
  b = 2
  with a as x, b as y:
    pass
  return x + y
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ContextManagers, AsyncWithAccepted) {
  const char* src = R"PY(
def f() -> int:
  a = 3
  async with a as x:
    pass
  return x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ContextManagers, WithoutAsStillSequentialScope) {
  const char* src = R"PY(
def f() -> int:
  with 1:
    z = 7
  return z
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ContextManagers, AsFromLiteralTypesAndUses) {
  const char* src = R"PY(
def f() -> int:
  with 41 as x:
    pass
  return x + 1
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ContextManagers, MixedItemsSomeWithoutAs) {
  const char* src = R"PY(
def f() -> int:
  a = 1
  b = 2
  with a, b as y:
    pass
  return y + 3
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ContextManagers, NestedWithBindsAndUses) {
  const char* src = R"PY(
def f() -> int:
  a = 5
  b = 6
  with a as x:
    with b as y:
      pass
  return x + y
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ContextManagers, AsyncWithMultipleItemsMixed) {
  const char* src = R"PY(
def f() -> int:
  a = 3
  b = 4
  async with a as x, b:
    pass
  return x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}
