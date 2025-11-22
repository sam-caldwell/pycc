/***
 * Name: test_codegen_platform_lowering
 * Purpose: Verify lowering of platform.system/machine/release/version.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="platm.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenPlatform, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = platform.system()
  b = platform.machine()
  c = platform.release()
  d = platform.version()
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_platform_system()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_platform_machine()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_platform_release()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_platform_version()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_platform_system()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_platform_machine()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_platform_release()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_platform_version()"), std::string::npos);
}

