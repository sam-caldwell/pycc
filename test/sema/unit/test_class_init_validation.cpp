/***
 * Name: test_class_init_validation
 * Purpose: Cover class construction argument validation against __init__.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "class_init_val.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ClassInitValidation, ArgTypeMismatchRejected) {
  const char* src = R"PY(
class C:
  def __init__(x: int) -> None:
    return None
def main() -> int:
  c = C('a')
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ClassInitValidation, ArityMismatchRejected) {
  const char* src = R"PY(
class C:
  def __init__(x: int, y: int) -> None:
    return None
def main() -> int:
  c = C(1)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

