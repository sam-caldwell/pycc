/***
 * Name: test_codegen_module_ctor
 * Purpose: Verify Codegen emits a void @pycc_module_init and @llvm.global_ctors for static initialization.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="mctor.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenModuleCtor, EmitsGlobalCtorsSingle) {
  const char* src = R"PY(
def main() -> int:
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("define void @pycc_module_init_0()"), std::string::npos);
  ASSERT_NE(ir.find("@llvm.global_ctors = appending global [1 x { i32, ptr, ptr } ] [{ i32 65535, ptr @pycc_module_init_0, ptr null }]"), std::string::npos);
}
