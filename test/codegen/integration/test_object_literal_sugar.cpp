/**
 * Name: test_object_literal_sugar
 * Purpose: Verify parser sugar object(...) lowers to ObjectLiteral and IR uses object runtime.
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

TEST(CodegenIR, ObjectLiteralSugar) {
  const char* src =
      "def main() -> int:\n"
      "  o = object(1, 2)\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  // Ensure AST uses ObjectLiteral at the call site by inspecting IR for object runtime calls
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("declare ptr @pycc_object_new(i64)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_object_new(i64 2)"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_object_set(ptr"), std::string::npos);
}

