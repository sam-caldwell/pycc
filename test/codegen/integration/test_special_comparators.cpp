/***
 * Name: test_special_comparators
 * Purpose: Verify IR for 'is'/'is not' comparators.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "spec_cmp.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, IsOnPointers_UsesPtrEq) {
  const char* src =
      "def main() -> bool:\n"
      "  s = \"a\"\n"
      "  t = s\n"
      "  return s is t\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("icmp eq ptr"), std::string::npos);
}

TEST(CodegenIR, IsNotOnInts_UsesICmpNe) {
  const char* src =
      "def main() -> bool:\n"
      "  return 1 is not 2\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("icmp ne i32 1, 2"), std::string::npos);
}

