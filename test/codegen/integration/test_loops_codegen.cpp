/***
 * Name: test_loops_codegen
 * Purpose: Verify IR lowering for while and for loops.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "loops.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, WhileAndForBasic) {
  const char* src =
      "def main() -> int:\n"
      "  s = 0\n"
      "  for x in [1,2,3]:\n"
      "    s = s + x\n"
      "  i = 0\n"
      "  while i < 2:\n"
      "    s = s + 1\n"
      "    i = i + 1\n"
      "  return s\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // For over list literal unrolled into stores
  ASSERT_NE(ir.find("store i32 1"), std::string::npos);
  ASSERT_NE(ir.find("store i32 2"), std::string::npos);
  ASSERT_NE(ir.find("store i32 3"), std::string::npos);
  // While CFG labels
  ASSERT_NE(ir.find("while.cond"), std::string::npos);
  ASSERT_NE(ir.find("while.body"), std::string::npos);
  ASSERT_NE(ir.find("while.end"), std::string::npos);
}

