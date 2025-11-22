/***
 * Name: test_codegen_android_support_lowering
 * Purpose: Verify lowering for _android_support helpers and declarations.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="android_support_codegen.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenAndroidSupport, DeclaresAndCalls) {
  const char* src = R"PY(
import _android_support
def main() -> int:
  a = _android_support.android_platform()
  b = _android_support.default_libdir()
  c = _android_support.ldflags()
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_android_platform()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_android_default_libdir()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_android_ldflags()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_android_platform()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_android_default_libdir()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_android_ldflags()"), std::string::npos);
}

