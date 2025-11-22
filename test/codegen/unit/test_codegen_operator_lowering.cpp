/***
 * Name: test_codegen_operator_lowering
 * Purpose: Verify lowering of operator module API.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="op.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenOperator, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = operator.add(1, 2)
  b = operator.sub(3, 1)
  c = operator.mul(2, 4)
  d = operator.truediv(1, 2)
  e = operator.neg(5)
  f = operator.eq(1, 1)
  g = operator.lt(1, 2)
  h = operator.not_(1)
  i = operator.truth(1)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_operator_add(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_operator_sub(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_operator_mul(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_operator_truediv(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_operator_neg(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_operator_eq(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_operator_lt(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_operator_not(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_operator_truth(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_operator_add(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_operator_sub(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_operator_mul(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_operator_truediv(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_operator_neg(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_operator_eq(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_operator_lt(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_operator_not(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_operator_truth(ptr"), std::string::npos);
}

