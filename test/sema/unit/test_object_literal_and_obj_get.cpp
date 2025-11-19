/***
 * Name: test_object_literal_and_obj_get
 * Purpose: Cover ObjectLiteral typing and obj_get builtin semantics.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "obj_get.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ObjectLiteral, ObjGetIndexIntOk) {
  const char* src = R"PY(
def f() -> int:
  o = object('a', 'b')
  x = obj_get(o, 1)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ObjectLiteral, ObjGetIndexMustBeInt) {
  const char* src = R"PY(
def f() -> int:
  o = object('a', 'b')
  x = obj_get(o, '1')
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

