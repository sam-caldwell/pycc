/***
 * Name: test_codegen_math_calls_all
 * Purpose: Drive IR generation through all math.* lowering branches (calls present).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="math_calls_all.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenMath, CallsAllCoreIntrinsics) {
  const char* src = R"PY(
import math
def main() -> int:
  a = math.fabs(-3.0)
  b = math.sin(1.0)
  c = math.cos(1.0)
  d = math.asin(0.5)
  e = math.acos(0.5)
  f = math.atan(1.0)
  g = math.exp(1.0)
  h = math.exp2(3.0)
  i = math.log(2.0)
  j = math.log2(8.0)
  k = math.log10(100.0)
  l = math.pow(2.0, 3.0)
  m = math.copysign(1.0, -2.0)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("call double @llvm.fabs.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.sin.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.cos.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.asin.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.acos.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.atan.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.exp.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.exp2.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.log.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.log2.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.log10.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.pow.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.copysign.f64(double"), std::string::npos);
}
