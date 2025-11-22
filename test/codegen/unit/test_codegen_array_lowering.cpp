/***
 * Name: test_codegen_array_lowering
 * Purpose: Verify lowering of array subset functions.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR_arr(const char* src, const char* file="arr.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenArray, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = array.array('i', [1,2])
  array.append(a, 3)
  x = array.pop(a)
  b = array.tolist(a)
  return 0
)PY";
  auto ir = genIR_arr(src);
  ASSERT_NE(ir.find("declare ptr @pycc_array_array(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_array_append(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_array_pop(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_array_tolist(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_array_array(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_array_append(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_array_pop(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_array_tolist(ptr"), std::string::npos);
}

