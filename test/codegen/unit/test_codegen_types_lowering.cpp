/***
 * Name: test_codegen_types_lowering
 * Purpose: Verify lowering of types.SimpleNamespace.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR_types(const char* src, const char* file="types_ns.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenTypes, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  ns = types.SimpleNamespace([['a', 1], ['b', 'x']])
  return 0
)PY";
  auto ir = genIR_types(src);
  ASSERT_NE(ir.find("declare ptr @pycc_types_simple_namespace(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_types_simple_namespace(ptr"), std::string::npos);
}

