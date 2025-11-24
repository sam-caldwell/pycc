/***
 * Name: test_codegen_eval_pow_and_shifts
 * Purpose: Extend compile-time eval coverage: '**' (int/float) and bit shifts.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="ee_pow_shifts.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenEvalSmallExpr, PowIntAndFloatAndShifts) {
  const char* src = R"PY(
def main() -> int:
  a = eval("2**3")
  b = eval("2.0**3")
  c = eval("8>>1")
  d = eval("1<<2")
  return 0
)PY";
  const auto ir = genIR(src);
  // Boxed int for 2**3 == 8
  ASSERT_NE(ir.find("call ptr @pycc_box_int(i64 8)"), std::string::npos);
  // Boxed float (2.0**3 == 8.0)
  ASSERT_NE(ir.find("@pycc_box_float"), std::string::npos);
  // Shifts produce boxed ints; at least one box_int call for shifts
  ASSERT_NE(ir.find("@pycc_box_int"), std::string::npos);
}

