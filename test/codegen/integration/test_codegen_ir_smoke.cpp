/***
 * Name: test_codegen_ir_smoke
 * Purpose: Smoke-test a few IR patterns: boxing and write barriers for list/object.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "smoke.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIRSmoke, BoxingAndBarriersForListAndObject) {
  const char* src =
      "def main() -> int:\n"
      "  l = [1, 2]\n"
      "  o = object(True, 3.5)\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // Declarations present
  ASSERT_NE(ir.find("declare ptr @pycc_box_int(i64)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_box_bool(i1)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_box_float(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_gc_write_barrier(ptr, ptr)"), std::string::npos);
  // List creation and push (boxing ints + list push)
  ASSERT_NE(ir.find("call ptr @pycc_list_new(i64 2)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_box_int(i64 1)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_box_int(i64 2)"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_list_push(ptr"), std::string::npos);
  // Object creation with boxed True and 3.5, and object_set stores
  ASSERT_NE(ir.find("call ptr @pycc_object_new(i64 2)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_box_bool(i1 true)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_box_float(double 3.5)"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_object_set(ptr"), std::string::npos);
  // Write barrier called at least once for pointer stores
  ASSERT_NE(ir.find("call void @pycc_gc_write_barrier(ptr"), std::string::npos);
}

