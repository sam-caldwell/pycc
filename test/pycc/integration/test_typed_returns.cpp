/***
 * Name: test_typed_returns
 * Purpose: Ensure functions can return bool and double with correct IR.
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

TEST(TypedReturns, BoolReturn) {
  const char* src =
      "def main() -> bool:\n"
      "  return True\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("define i1 @main()"), std::string::npos);
  ASSERT_NE(ir.find("ret i1 true"), std::string::npos);
}

TEST(TypedReturns, FloatReturn) {
  const char* src =
      "def f() -> float:\n"
      "  return 1.5\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("define double @f()"), std::string::npos);
  ASSERT_NE(ir.find("ret double"), std::string::npos);
}
