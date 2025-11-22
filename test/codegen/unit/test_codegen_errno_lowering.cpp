/***
 * Name: test_codegen_errno_lowering
 * Purpose: Verify lowering of errno constants as functions.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="errno.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenErrno, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = errno.EPERM()
  b = errno.ENOENT()
  c = errno.EEXIST()
  d = errno.EISDIR()
  e = errno.ENOTDIR()
  f = errno.EACCES()
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare i32 @pycc_errno_EPERM()"), std::string::npos);
  ASSERT_NE(ir.find("declare i32 @pycc_errno_ENOENT()"), std::string::npos);
  ASSERT_NE(ir.find("declare i32 @pycc_errno_EEXIST()"), std::string::npos);
  ASSERT_NE(ir.find("declare i32 @pycc_errno_EISDIR()"), std::string::npos);
  ASSERT_NE(ir.find("declare i32 @pycc_errno_ENOTDIR()"), std::string::npos);
  ASSERT_NE(ir.find("declare i32 @pycc_errno_EACCES()"), std::string::npos);
  ASSERT_NE(ir.find("call i32 @pycc_errno_EPERM()"), std::string::npos);
}

