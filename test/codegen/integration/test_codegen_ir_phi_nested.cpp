/***
 * Name: test_codegen_ir_phi_nested
 * Purpose: Ensure phi nodes are formed in nested if/else returns via short-circuit expressions.
 */
#include <gtest/gtest.h>
#include <string>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "phi.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, PhiNodesInNestedIfElseReturns) {
  const char* src =
      "def f(a: bool, b: bool, c: bool) -> bool:\n"
      "  if a:\n"
      "    if b:\n"
      "      return c and a\n"
      "    else:\n"
      "      return b or c\n"
      "  else:\n"
      "    return b and c\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // Function signature and returns on i1
  ASSERT_NE(ir.find("define i1 @f(i1 %a, i1 %b, i1 %c)"), std::string::npos);
  ASSERT_NE(ir.find("ret i1"), std::string::npos);
  // Nested if labels should exist
  ASSERT_NE(ir.find("if.then"), std::string::npos);
  ASSERT_NE(ir.find("if.end"), std::string::npos);
  // Short-circuit lowers into blocks with phi i1 merges
  // Expect at least one 'and.end' and one 'or.end' with phi
  ASSERT_NE(ir.find("and.end"), std::string::npos);
  ASSERT_NE(ir.find("or.end"), std::string::npos);
  // Presence of phi nodes for booleans
  size_t pos = 0; int phiCount = 0;
  while ((pos = ir.find("phi i1", pos)) != std::string::npos) { ++phiCount; pos += 6; }
  ASSERT_GE(phiCount, 2);
}

