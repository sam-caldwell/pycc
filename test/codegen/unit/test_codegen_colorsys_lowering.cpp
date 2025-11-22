/***
 * Name: test_codegen_colorsys_lowering
 * Purpose: Verify lowering of colorsys.rgb_to_hsv/hsv_to_rgb.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR_cs(const char* src, const char* file="cs.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenColorsys, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = colorsys.rgb_to_hsv(1.0, 0.0, 0.0)
  b = colorsys.hsv_to_rgb(0.0, 1.0, 1.0)
  return 0
)PY";
  auto ir = genIR_cs(src);
  ASSERT_NE(ir.find("declare ptr @pycc_colorsys_rgb_to_hsv(double, double, double)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_colorsys_hsv_to_rgb(double, double, double)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_colorsys_rgb_to_hsv(double"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_colorsys_hsv_to_rgb(double"), std::string::npos);
}

