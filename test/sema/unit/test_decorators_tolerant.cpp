/***
 * Name: test_decorators_tolerant
 * Purpose: Ensure unknown decorators do not cause Sema failures; body still type-checked.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="decorators.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaDecorators, UnknownFunctionDecoratorTolerated) {
  const char* src = R"PY(
@log
def f() -> int:
  return 0
def main() -> int:
  return f()
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaDecorators, UnknownClassDecoratorTolerated) {
  const char* src = R"PY(
@decor
class C:
  pass
def main() -> int:
  return 0
)PY";
  auto mod = parseSrc(src, "cls_decor.py");
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

