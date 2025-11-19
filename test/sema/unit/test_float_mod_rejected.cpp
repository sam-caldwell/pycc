/***
 * Name: test_float_mod_rejected
 * Purpose: Ensure float modulo is rejected by Sema typing.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "fmod.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaFloatMod, ModOnFloatFails) {
  const char* src = R"PY(
def f(a: float, b: float) -> float:
  return a % b
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

