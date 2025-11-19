/***
 * Name: test_exceptions_codegen
 * Purpose: Verify IR lowering for try/except/else/finally follows linearized flow in this subset.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "try.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, TryExceptElseFinally_LinearLowering) {
  const char* src =
      "def main() -> int:\n"
      "  x = 0\n"
      "  try:\n"
      "    x = 1\n"
      "  except Exception as e:\n"
      "    x = 2\n"
      "  else:\n"
      "    x = x + 1\n"
      "  finally:\n"
      "    y = 4\n"
      "  return x\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // Expect assignments for try body (x=1) and else (x=x+1) to appear in IR
  ASSERT_NE(ir.find("store i32 1"), std::string::npos);
  ASSERT_NE(ir.find("add i32"), std::string::npos);
}

