/***
 * Name: test_dict_unpack_semantics
 * Purpose: Cover DictLiteral unpacks and ensure they are analyzed in sema.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "dict_unpack.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(DictUnpack, DictLiteralWithUnpackAccepted) {
  const char* src = R"PY(
def f() -> int:
  d = {'a': 1, **{'b': 2}}
  return d['a']
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

