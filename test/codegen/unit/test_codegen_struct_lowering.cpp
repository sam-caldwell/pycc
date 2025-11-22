/***
 * Name: test_codegen_struct_lowering
 * Purpose: Verify lowering of struct.pack/unpack/calcsize.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR_st(const char* src, const char* file="st.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenStruct, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  b = struct.pack('<i', [1])
  l = struct.unpack('<i', b)
  n = struct.calcsize('<i')
  return 0
)PY";
  auto ir = genIR_st(src);
  ASSERT_NE(ir.find("declare ptr @pycc_struct_pack(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_struct_unpack(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i32 @pycc_struct_calcsize(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_struct_pack(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_struct_unpack(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i32 @pycc_struct_calcsize(ptr"), std::string::npos);
}

