/**
 * Name: test_codegen_recursion
 * Purpose: Verify IR for simple recursion (factorial) — define + recursive call present.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "recur.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenRecursion, FactorialIRContainsRecursiveCall) {
  const char* src =
      "def fact(n: int) -> int:\n"
      "  if n == 0:\n"
      "    return 1\n"
      "  else:\n"
      "    return n * fact(n - 1)\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("define i32 @fact(i32 %n)"), std::string::npos);
  ASSERT_NE(ir.find("call i32 @fact(i32"), std::string::npos);
}

