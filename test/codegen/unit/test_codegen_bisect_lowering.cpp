/***
 * Name: test_codegen_bisect_lowering
 * Purpose: Verify lowering of bisect.bisect_left/right.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="bis.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenBisect, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = bisect.bisect_left([1,2,3], 2)
  b = bisect.bisect_right([1,2,3], 2)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare i32 @pycc_bisect_left(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i32 @pycc_bisect_right(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call i32 @pycc_bisect_left(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i32 @pycc_bisect_right(ptr"), std::string::npos);
}

