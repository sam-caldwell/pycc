/***
 * Name: test_codegen_module_ctors_multi
 * Purpose: Verify multiple module init functions are emitted in deterministic order and listed in @llvm.global_ctors.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

TEST(CodegenModuleCtors, EmitsTwoInitsInOrder) {
  lex::Lexer L;
  L.pushString("def a() -> int:\n  return 0\n", "b.py");
  L.pushString("def b() -> int:\n  return 0\n", "a.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  auto ir = codegen::Codegen::generateIR(*mod);
  // Two init functions and array with 2 entries
  ASSERT_NE(ir.find("define void @pycc_module_init_0()"), std::string::npos);
  ASSERT_NE(ir.find("define void @pycc_module_init_1()"), std::string::npos);
  ASSERT_NE(ir.find("@llvm.global_ctors = appending global [2 x { i32, ptr, ptr } ]"), std::string::npos);
  // Ensure order is stable (index 0 then 1 in the array)
  const auto pos0 = ir.find("{ i32 65535, ptr @pycc_module_init_0, ptr null }");
  const auto pos1 = ir.find("{ i32 65535, ptr @pycc_module_init_1, ptr null }");
  ASSERT_NE(pos0, std::string::npos);
  ASSERT_NE(pos1, std::string::npos);
  ASSERT_LT(pos0, pos1);
}

