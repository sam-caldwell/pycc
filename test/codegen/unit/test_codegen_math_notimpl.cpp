/***
 * Name: test_codegen_math_notimpl
 * Purpose: Verify unknown math attribute lowers to a runtime raise with NotImplementedError.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="math_notimpl.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenMath, UnknownAttrLowersRaise) {
  const char* src = R"PY(
import math
def main() -> int:
  a = math.not_a_func(1)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("pycc_rt_raise"), std::string::npos);
}
