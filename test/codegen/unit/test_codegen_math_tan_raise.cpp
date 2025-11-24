/***
 * Name: test_codegen_math_tan_raise
 * Purpose: Verify math.tan lowering computes sin/cos and also raises NotImplemented.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="math_tan_ri.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenMath, TanLowersAndRaises) {
  const char* src = R"PY(
import math
def main() -> int:
  x = math.tan(1.0)
  return 0
)PY";
  auto ir = genIR(src);
  // Sin and cos are used to compute tan
  ASSERT_NE(ir.find("call double @llvm.sin.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.cos.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("fdiv double"), std::string::npos);
  // NotImplementedError raise emitted for tan
  ASSERT_NE(ir.find("call void @pycc_rt_raise(ptr"), std::string::npos);
}

