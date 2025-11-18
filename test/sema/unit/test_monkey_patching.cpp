/***
 * Name: test_monkey_patching
 * Purpose: Validate monkey-patching semantics allowed within known code as polymorphism.
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

TEST(MonkeyPatching, AliasSingleFunctionAndCall) {
  const char* src = R"PY(
def f(x: int) -> int:
  return x
def main() -> int:
  h = f
  return h(2)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

TEST(MonkeyPatching, AliasTwoWithSameSignatureOkay) {
  const char* src = R"PY(
def f(x: int) -> int:
  return x
def g(x: int) -> int:
  return x
def main() -> int:
  h = f
  h = g
  return h(3)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

TEST(MonkeyPatching, AliasIncompatibleArgumentFails) {
  const char* src = R"PY(
def g(x: str) -> int:
  return 0
def main() -> int:
  h = g
  return h(2)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(MonkeyPatching, MixedSignaturesRejected) {
  const char* src = R"PY(
def f(x: int) -> int:
  return x
def g(x: str) -> int:
  return 0
def main() -> int:
  h = f
  h = g
  return h(1)
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(MonkeyPatching, UnknownTargetNotAllowed) {
  const char* src = R"PY(
def main() -> int:
  h = not_known
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}
