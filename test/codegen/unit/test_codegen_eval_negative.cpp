/***
 * Name: test_codegen_eval_negative
 * Purpose: Ensure unsupported eval expressions safely return a null placeholder.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="eeneg.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenEvalNegative, UnsupportedTypesYieldNull) {
  const char* src = R"PY(
def main() -> int:
  a = eval("1 and []")
  return 0
)PY";
  const auto ir = genIR(src);
  // Expect that the result of eval is the ptr constant 'null' and not a boxed value
  ASSERT_NE(ir.find("store ptr null"), std::string::npos);
  ASSERT_EQ(ir.find("@pycc_box_int"), std::string::npos);
  ASSERT_EQ(ir.find("@pycc_box_bool"), std::string::npos);
  ASSERT_EQ(ir.find("@pycc_box_float"), std::string::npos);
}

