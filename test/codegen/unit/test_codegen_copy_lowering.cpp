/***
 * Name: test_codegen_copy_lowering
 * Purpose: Verify lowering of copy.copy/deepcopy.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="cpy.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenCopy, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = copy.copy([1,2,3])
  b = copy.deepcopy({"x": [1]})
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_copy_copy(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_copy_deepcopy(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_copy_copy(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_copy_deepcopy(ptr"), std::string::npos);
}

