/***
 * Name: test_codegen_stdlib_more
 * Purpose: Extra IR checks for stdlib lowering (math casts and other modules stubs).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="stdlib_more.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenStdlibMore, MathFloorCastsIntToDoubleAndBack) {
  const char* src = R"PY(
import math
def main() -> int:
  v = math.floor(3)
  return 0
)PY";
  auto ir = genIR(src);
  // i32 -> double cast (sitofp) and floor call, then fptosi back to i32
  ASSERT_NE(ir.find("declare double @llvm.floor.f64(double)"), std::string::npos);
  ASSERT_NE(ir.find("sitofp i32"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.floor.f64(double"), std::string::npos);
  ASSERT_NE(ir.find("fptosi double"), std::string::npos);
}

TEST(CodegenStdlibMore, TimeSleepLowered) {
  const char* src = R"PY(
import time
def main() -> int:
  time.sleep(1)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare void @pycc_time_sleep(double)"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_time_sleep(double"), std::string::npos);
}
