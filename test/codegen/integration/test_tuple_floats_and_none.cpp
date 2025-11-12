/***
 * Name: test_tuple_floats_and_none
 * Purpose: Verify tuple returns with floats/mixed and None comparisons.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, TupleReturn_Floats) {
  const char* src =
      "def tfloat() -> tuple:\n"
      "  return (1.5, 2.25)\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("define { double, double } @tfloat()"), std::string::npos);
  ASSERT_NE(ir.find("ret { double, double }"), std::string::npos);
}

TEST(CodegenIR, TupleReturn_Mixed) {
  const char* src =
      "def tmix() -> tuple:\n"
      "  return (1, 2.0)\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("define { i32, double } @tmix()"), std::string::npos);
  ASSERT_NE(ir.find("ret { i32, double }"), std::string::npos);
}

TEST(CodegenIR, CompareNoneConst) {
  const char* src =
      "def c1() -> bool:\n"
      "  return 1 == None\n"
      "def c2() -> bool:\n"
      "  return 1 != None\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("define i1 @c1()"), std::string::npos);
  ASSERT_NE(ir.find("ret i1 false"), std::string::npos);
  ASSERT_NE(ir.find("define i1 @c2()"), std::string::npos);
  ASSERT_NE(ir.find("ret i1 true"), std::string::npos);
}

