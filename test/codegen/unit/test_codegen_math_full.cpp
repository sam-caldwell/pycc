/***
 * Name: test_codegen_math_full
 * Purpose: Verify full lowering for math stdlib functions to LLVM intrinsics/IR.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="math_full.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenMath, DeclaresIntrinsics) {
  const char* src = R"PY(
import math
def main() -> int:
  a = math.sqrt(4)
  b = math.floor(3.14)
  c = math.ceil(3.14)
  d = math.trunc(3.14)
  e = math.fabs(-3.0)
  f = math.sin(1.0)
  g = math.cos(1.0)
  h = math.tan(1.0)
  i = math.asin(0.5)
  j = math.acos(0.5)
  k = math.atan(1.0)
  l = math.atan2(1.0, 1.0)
  m = math.exp(1.0)
  n = math.exp2(3.0)
  o = math.log(2.0)
  p = math.log2(8.0)
  q = math.log10(100.0)
  r = math.pow(2.0, 3.0)
  s = math.fmod(5.0, 2.0)
  t = math.copysign(1.0, -2.0)
  u = math.hypot(3.0, 4.0)
  v = math.degrees(3.141592653589793)
  w = math.radians(180.0)
  return 0
)PY";
  auto ir = genIR(src);
  // Declarations
  ASSERT_NE(ir.find("declare double @llvm.sqrt.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.floor.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.ceil.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.trunc.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.fabs.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.sin.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.cos.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.asin.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.acos.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.atan.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.atan2.f64(double, double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.exp.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.exp2.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.log.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.log2.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.log10.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.copysign.f64(double, double)"), std::string::npos);
}

TEST(CodegenMath, CallsAndCastsPresent) {
  const char* src = R"PY(
import math
def main() -> int:
  a = math.sqrt(4)
  b = math.floor(3.14)
  c = math.ceil(3.14)
  d = math.trunc(3.14)
  e = math.tan(1.0)
  f = math.atan2(1.0, 1.0)
  g = math.fmod(5.0, 2.0)
  h = math.hypot(3.0, 4.0)
  i = math.degrees(3.141592653589793)
  j = math.radians(180.0)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("call double @llvm.sqrt.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.floor.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.ceil.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.trunc.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("fptosi double"), std::string::npos); // casts for floor/ceil/trunc
  // tan implemented as sin/cos + fdiv
  ASSERT_NE(ir.find("call double @llvm.sin.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.cos.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("fdiv double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.atan2.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("frem double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.sqrt.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("fmul double"), std::string::npos);
}
