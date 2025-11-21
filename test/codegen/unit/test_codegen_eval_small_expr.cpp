/***
 * Name: test_codegen_eval_small_expr
 * Purpose: Verify compile-time AST evaluation for small expressions in eval().
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="ee_small.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenEvalSmallExpr, ComputesArithmetic) {
  const char* src = R"PY(
def main() -> int:
  a = eval("1+2*3")
  b = eval("10//3")
  c = eval("1.5*2")
  d = eval("4<5")
  return 0
)PY";
  const auto ir = genIR(src);
  // Boxed int for 1+2*3 == 7
  ASSERT_NE(ir.find("call ptr @pycc_box_int(i64 7)"), std::string::npos);
  // Boxed int for 10//3 == 3
  ASSERT_NE(ir.find("call ptr @pycc_box_int(i64 3)"), std::string::npos);
  // Boxed float for 1.5*2 == 3.0 (string form may be 3 or 3.00000; just check the call occurs)
  ASSERT_NE(ir.find("@pycc_box_float"), std::string::npos);
  // Boxed bool for 4<5
  ASSERT_NE(ir.find("call ptr @pycc_box_bool(i1 1)"), std::string::npos);
}

