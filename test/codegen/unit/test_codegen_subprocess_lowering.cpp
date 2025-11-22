/***
 * Name: test_codegen_subprocess_lowering
 * Purpose: Verify lowering for subprocess.run/call/check_call and declarations.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="sp.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenSubprocess, DeclaresAndCalls) {
  const char* src = R"PY(
import subprocess
def main() -> int:
  a = subprocess.run("true")
  b = subprocess.call("true")
  c = subprocess.check_call("true")
  return a+b+c
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare i32 @pycc_subprocess_run(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i32 @pycc_subprocess_call(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i32 @pycc_subprocess_check_call(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call i32 @pycc_subprocess_run(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i32 @pycc_subprocess_check_call(ptr"), std::string::npos);
}

