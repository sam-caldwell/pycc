/***
 * Name: test_descriptors_arity
 * Purpose: Cover descriptor dunder arity checks for __set__ and __delete__.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "desc_arity.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(DescriptorsArity, SetAndDeleteArityChecked) {
  const char* src = R"PY(
class X:
  def __set__(a: int) -> int:
    return 0
class Y:
  def __delete__(a: int, b: int) -> int:
    return 0
def main() -> int:
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

