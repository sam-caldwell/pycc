/***
 * Name: test_strings_codegen
 * Purpose: Verify codegen lowers string vars and len(str) via runtime charlen.
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

TEST(CodegenIR, LenOfStringVarCallsCharLen) {
  const char* src =
      "def main() -> int:\n"
      "  s = \"abcd\"\n"
      "  return len(s)\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("declare i64 @pycc_string_charlen(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call i64 @pycc_string_charlen(ptr"), std::string::npos);
}
