/***
 * Name: test_bitwise_and_shifts
 * Purpose: Verify IR for bitwise ops (&|^) and shifts (<< >>) on ints.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "bitwise.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, BitwiseAndOrXorShifts) {
  const char* src =
      "def main() -> int:\n"
      "  a = 5 & 3\n"
      "  b = 5 | 2\n"
      "  c = 5 ^ 1\n"
      "  d = 1 << 3\n"
      "  e = 8 >> 2\n"
      "  return a\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("and i32"), std::string::npos);
  ASSERT_NE(ir.find("or i32"), std::string::npos);
  ASSERT_NE(ir.find("xor i32"), std::string::npos);
  ASSERT_NE(ir.find("shl i32"), std::string::npos);
  ASSERT_NE(ir.find("ashr i32"), std::string::npos);
}

