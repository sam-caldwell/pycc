/***
 * Name: test_codegen_bisect_insort_lowering
 * Purpose: Verify lowering of bisect.bisect alias and insort_*.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR_bi(const char* src, const char* file="bis2.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenBisect, DeclaresAndCallsInsort) {
  const char* src = R"PY(
def main() -> int:
  a = bisect.bisect([1,2,3], 2)
  bisect.insort_left([1,2,3], 2)
  bisect.insort_right([1,2,3], 2)
  bisect.insort([1,2,3], 2)
  return 0
)PY";
  auto ir = genIR_bi(src);
  ASSERT_NE(ir.find("declare i32 @pycc_bisect_left(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i32 @pycc_bisect_right(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_bisect_insort_left(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_bisect_insort_right(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call i32 @pycc_bisect_right(ptr"), std::string::npos); // bisect alias
  ASSERT_NE(ir.find("call void @pycc_bisect_insort_left(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_bisect_insort_right(ptr"), std::string::npos);
}

