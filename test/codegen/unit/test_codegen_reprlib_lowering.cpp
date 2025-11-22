/***
 * Name: test_codegen_reprlib_lowering
 * Purpose: Verify lowering of reprlib.repr.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR_rp(const char* src, const char* file="rp.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenReprlib, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  s = reprlib.repr([1,2,3])
  return 0
)PY";
  auto ir = genIR_rp(src);
  ASSERT_NE(ir.find("declare ptr @pycc_reprlib_repr(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_reprlib_repr(ptr"), std::string::npos);
}

