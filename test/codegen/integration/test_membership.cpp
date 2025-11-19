/***
 * Name: test_membership
 * Purpose: Verify IR for 'in'/'not in' with list/tuple literals.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "membership.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, In_ListLiteral_BuildsOrOfEquals) {
  const char* src =
      "def main() -> bool:\n"
      "  return 2 in [1,2,3]\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("icmp eq i32 2, 1"), std::string::npos);
  ASSERT_NE(ir.find("icmp eq i32 2, 2"), std::string::npos);
  ASSERT_NE(ir.find("or i1"), std::string::npos);
}

TEST(CodegenIR, NotIn_TupleLiteral_XorsTrue) {
  const char* src =
      "def main() -> bool:\n"
      "  return 4 not in (1,2,3)\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("icmp eq i32 4, 1"), std::string::npos);
  ASSERT_NE(ir.find("xor i1"), std::string::npos);
}

