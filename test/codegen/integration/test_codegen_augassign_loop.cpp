/***
 * Name: test_codegen_augassign_loop
 * Purpose: Verify codegen emits augassign arithmetic and while break/continue branches.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "augloop.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, AugAssignAndLoopControl) {
  const char* src =
      "def main() -> int:\n"
      "  x = 1\n"
      "  x += 2\n"
      "  while x:\n"
      "    x -= 1\n"
      "    break\n"
      "  return x\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // Expect an add and a sub emitted for augassigns
  ASSERT_NE(ir.find(" = add i32 "), std::string::npos);
  ASSERT_NE(ir.find(" = sub i32 "), std::string::npos);
  // Expect while labels and branch to end label from break
  ASSERT_NE(ir.find("while.cond"), std::string::npos);
  ASSERT_NE(ir.find("while.end"), std::string::npos);
}

