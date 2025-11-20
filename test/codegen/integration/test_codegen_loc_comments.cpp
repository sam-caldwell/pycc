/***
 * Name: test_codegen_loc_comments
 * Purpose: Ensure semantically tagged loc comments are present in IR (assign/return).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrcLoc(const char* src) {
  lex::Lexer L; L.pushString(src, "loc_test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, LocCommentsIncludeFileLineAndKind) {
  const char* src =
      "def main() -> int:\n"
      "  x = 7\n"
      "  return x\n";
  auto mod = parseSrcLoc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("; loc: loc_test.py:"), std::string::npos);
  ASSERT_NE(ir.find("(assign)"), std::string::npos);
  ASSERT_NE(ir.find("(return)"), std::string::npos);
}

