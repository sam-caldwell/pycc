/***
 * Name: test_codegen_shutil_lowering
 * Purpose: Verify lowering of shutil.copyfile/copy.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="shumod.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenShutil, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = shutil.copyfile("a.txt", "b.txt")
  b = shutil.copy("b.txt", "c.txt")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare i1 @pycc_shutil_copyfile(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_shutil_copy(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_shutil_copyfile(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_shutil_copy(ptr"), std::string::npos);
}

