/***
 * Name: test_codegen_closures_env
 * Purpose: Verify closure/env emission for functions with nonlocal captures.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src) {
  lex::Lexer L; L.pushString(src, "clos.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenClosures, EnvStructAllocaPresent) {
  const char* src = R"PY(
def f() -> int:
  x = 1
  def g() -> int:
    nonlocal x
    return x
  return x
)PY";
  const auto ir = genIR(src);
  // Captured environment alloca comment and symbol
  ASSERT_NE(ir.find("; env for function 'g' captures: x"), std::string::npos);
  ASSERT_NE(ir.find("%env.g = alloca { ptr }"), std::string::npos);
}

