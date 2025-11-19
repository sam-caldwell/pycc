/***
 * Name: test_unary_bitnot
 * Purpose: Verify IR for unary bitwise not (~) on ints.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "unary.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, UnaryBitNot) {
  const char* src =
      "def main() -> int:\n"
      "  return ~5\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // Expect xor i32 5, -1
  ASSERT_NE(ir.find("xor i32 5, -1"), std::string::npos);
}

