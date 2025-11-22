/***
 * Name: test_codegen_aix_support_lowering
 * Purpose: Verify lowering for _aix_support helpers and declarations.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="aix_support_codegen.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenAIXSupport, DeclaresAndCalls) {
  const char* src = R"PY(
import _aix_support
def main() -> int:
  a = _aix_support.aix_platform()
  b = _aix_support.default_libpath()
  c = _aix_support.ldflags()
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_aix_platform()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_aix_default_libpath()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_aix_ldflags()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_aix_platform()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_aix_default_libpath()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_aix_ldflags()"), std::string::npos);
}

