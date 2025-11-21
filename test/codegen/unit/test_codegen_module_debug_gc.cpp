/***
 * Name: test_codegen_module_debug_gc
 * Purpose: Ensure module init, GC strategy, and debug metadata are present in IR.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src) {
  lex::Lexer L; L.pushString(src, "moddbg.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenIR, ModuleInitAndDebugAndGCStrategy) {
  const char* src = R"PY(
def main() -> int:
  x = 7
  return x
)PY";
  const auto ir = genIR(src);
  // Module init symbol
  ASSERT_NE(ir.find("define i32 @pycc_module_init()"), std::string::npos);
  // GC strategy on function
  ASSERT_NE(ir.find(" gc \"shadow-stack\" personality ptr @__gxx_personality_v0"), std::string::npos);
  // Debug metadata declarations and compile unit
  ASSERT_NE(ir.find("@llvm.dbg.declare"), std::string::npos);
  ASSERT_NE(ir.find("!DICompileUnit"), std::string::npos);
}

