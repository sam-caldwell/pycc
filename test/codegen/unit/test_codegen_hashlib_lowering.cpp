/***
 * Name: test_codegen_hashlib_lowering
 * Purpose: Verify lowering of hashlib.sha256/md5 calls.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="hl.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenHashlib, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = hashlib.sha256("hello")
  b = hashlib.md5("hello")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_hashlib_sha256(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_hashlib_md5(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_hashlib_sha256(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_hashlib_md5(ptr"), std::string::npos);
}

