/***
 * Name: test_codegen_sys_lowering
 * Purpose: Verify sys.* lowering to runtime shims and declarations.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="sys_full.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenSys, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = sys.platform()
  b = sys.version()
  c = sys.maxsize()
  sys.exit(0)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_sys_platform()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_sys_version()"), std::string::npos);
  ASSERT_NE(ir.find("declare i64 @pycc_sys_maxsize()"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_sys_exit(i32)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_sys_platform()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_sys_version()"), std::string::npos);
  ASSERT_NE(ir.find("call i64 @pycc_sys_maxsize()"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_sys_exit(i32 0)"), std::string::npos);
}

