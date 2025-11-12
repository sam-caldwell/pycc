/***
 * Name: test_tuple_and_builtins
 * Purpose: Verify tuple returns and builtin lowering for len/isinstance.
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

TEST(CodegenIR, TupleReturn_IntInt) {
  const char* src =
      "def pair() -> tuple:\n"
      "  return (1, 2)\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // Function returns a struct { i32, i32 }
  ASSERT_NE(ir.find("define { i32, i32 } @pair()"), std::string::npos);
  ASSERT_NE(ir.find("ret { i32, i32 }"), std::string::npos);
}

TEST(CodegenIR, LenOfTupleLiteral) {
  const char* src =
      "def main() -> int:\n"
      "  return len((1,2,3))\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // Expect direct ret i32 3
  ASSERT_NE(ir.find("ret i32 3"), std::string::npos);
}

TEST(CodegenIR, IsInstanceParamInt) {
  const char* src =
      "def check(a: int) -> bool:\n"
      "  return isinstance(a, int)\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // Expect constant true return
  ASSERT_NE(ir.find("define i1 @check(i32 %a)"), std::string::npos);
  ASSERT_NE(ir.find("ret i1 true"), std::string::npos);
}

TEST(CodegenIR, IsInstanceFlowRefinement) {
  const char* src =
      "def f(a: int) -> int:\n"
      "  if isinstance(a, int):\n"
      "    return a\n"
      "  else:\n"
      "    return 0\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("define i32 @f(i32 %a)"), std::string::npos);
  ASSERT_NE(ir.find("br i1 true"), std::string::npos);
}

TEST(CodegenIR, LenOfStringLiteral) {
  const char* src =
      "def main() -> int:\n"
      "  return len(\"abcd\")\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("ret i32 4"), std::string::npos);
}

TEST(CodegenIR, TupleReturn_Int3) {
  const char* src =
      "def t3() -> tuple:\n"
      "  return (1,2,3)\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("define { i32, i32, i32 } @t3()"), std::string::npos);
  ASSERT_NE(ir.find("ret { i32, i32, i32 }"), std::string::npos);
}
