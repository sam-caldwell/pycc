/***
 * Name: test_codegen_stdlib_dispatch
 * Purpose: Verify stdlib attribute dispatch lowers math functions and stubs others.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="stdlib.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenStdlib, LowersMathSqrtFloorPow) {
  const char* src = R"PY(
import math
def main() -> int:
  a = math.sqrt(9)
  b = math.floor(3.14)
  c = math.pow(2, 3)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare double @llvm.sqrt.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.sqrt.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.floor.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.floor.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("declare double @llvm.pow.f64(double, double)"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.pow.f64(double"), std::string::npos);
}

TEST(CodegenStdlib, StubsUnimplementedWithRaise) {
  const char* src = R"PY(
import math
def main() -> int:
  x = math.tan(1.0)
  return 0
)PY";
  auto ir = genIR(src);
  // Ensure a call to runtime raise is generated for stubbed functions
  ASSERT_NE(ir.find("declare void @pycc_rt_raise(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_rt_raise(ptr"), std::string::npos);
}

