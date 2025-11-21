/***
 * Name: test_codegen_eval_exec
 * Purpose: Verify eval/exec literal lowering: eval("123"), eval("3.14"), eval("True") box to runtime; exec no-op.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="ee.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenEvalExec, BoxesNumericAndBoolLiterals) {
  const char* src = R"PY(
def main() -> int:
  a = eval("123")
  b = eval("3.14")
  c = eval("True")
  exec("x=1")
  return 0
)PY";
  const auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_box_int"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_box_float"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_box_bool"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_box_int(i64 123)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_box_float(double 3.14)"), std::string::npos);
  // bool boxing may appear as i1 1
  ASSERT_NE(ir.find("call ptr @pycc_box_bool(i1 1)"), std::string::npos);
}

