/***
 * Name: test_pow_floordiv
 * Purpose: Verify IR for '**' and '//' on ints and floats.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "powdiv.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, FloorDiv_Ints) {
  const char* src =
      "def main() -> int:\n"
      "  return 7 // 2\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("sdiv i32 7, 2"), std::string::npos);
}

TEST(CodegenIR, Pow_Ints_UsesPowiAndCast) {
  const char* src =
      "def main() -> int:\n"
      "  return 2 ** 3\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("call double @llvm.powi.f64"), std::string::npos);
  ASSERT_NE(ir.find("fptosi double"), std::string::npos);
}

TEST(CodegenIR, FloorDiv_Floats) {
  const char* src =
      "def f() -> float:\n"
      "  return 7.5 // 2.0\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("fdiv double"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.floor.f64"), std::string::npos);
}

TEST(CodegenIR, Pow_Float_And_Powi) {
  const char* src =
      "def g() -> float:\n"
      "  return 1.5 ** 2\n"
      "def h() -> float:\n"
      "  return 1.5 ** 2.5\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // powi for int exponent, pow for float exponent
  ASSERT_NE(ir.find("call double @llvm.powi.f64(double %"), std::string::npos);
  ASSERT_NE(ir.find("call double @llvm.pow.f64(double %"), std::string::npos);
}

