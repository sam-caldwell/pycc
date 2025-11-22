/***
 * Name: test_codegen__ast_lowering
 * Purpose: Verify lowering and declarations for _ast helpers.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="_ast_codegen.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(Codegen_Ast, DeclaresAndCalls) {
  const char* src = R"PY(
import _ast
def main() -> int:
  s = _ast.dump("x")
  it = _ast.iter_fields("x")
  w = _ast.walk("x")
  c = _ast.copy_location("new", "old")
  f = _ast.fix_missing_locations("n")
  d = _ast.get_docstring("n")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_ast_dump(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_ast_iter_fields(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_ast_walk(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_ast_copy_location(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_ast_fix_missing_locations(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_ast_get_docstring(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_ast_dump(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_ast_iter_fields(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_ast_walk(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_ast_copy_location(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_ast_fix_missing_locations(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_ast_get_docstring(ptr"), std::string::npos);
}

