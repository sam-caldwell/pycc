/***
 * Name: test_codegen_apple_support_lowering
 * Purpose: Verify lowering for _apple_support helpers and declarations.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="apple_support_codegen.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenAppleSupport, DeclaresAndCalls) {
  const char* src = R"PY(
import _apple_support
def main() -> int:
  a = _apple_support.apple_platform()
  b = _apple_support.default_sdkroot()
  c = _apple_support.ldflags()
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_apple_platform()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_apple_default_sdkroot()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_apple_ldflags()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_apple_platform()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_apple_default_sdkroot()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_apple_ldflags()"), std::string::npos);
}

