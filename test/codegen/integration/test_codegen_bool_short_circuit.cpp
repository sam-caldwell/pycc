/**
 * Name: test_codegen_bool_short_circuit
 * Purpose: Verify short-circuit lowering for and/or with int truthiness and not on int/float.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "bool_sc.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenBool, ShortCircuitIntAndOr) {
  const char* src =
      "def main() -> bool:\n"
      "  a = 0\n"
      "  b = 1\n"
      "  c = a and (b == 1)\n"
      "  d = a or (b == 1)\n"
      "  return d\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // int-to-bool conversion should appear (icmp ne i32 ... , 0)
  ASSERT_NE(ir.find("icmp ne i32"), std::string::npos);
  // short-circuit blocks with phi
  ASSERT_NE(ir.find("and.end"), std::string::npos);
  ASSERT_NE(ir.find("or.end"), std::string::npos);
  ASSERT_NE(ir.find("phi i1"), std::string::npos);
}

TEST(CodegenBool, NotOnIntAndFloat) {
  const char* src =
      "def f(x: int, y: float) -> bool:\n"
      "  a = not x\n"
      "  b = not y\n"
      "  return a or b\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // int truthiness -> icmp ne i32
  ASSERT_NE(ir.find("icmp ne i32"), std::string::npos);
  // float truthiness -> fcmp one double ... , 0.0
  ASSERT_NE(ir.find("fcmp one double"), std::string::npos);
  // not lowering via xor on both
  ASSERT_NE(ir.find("xor i1"), std::string::npos);
}
