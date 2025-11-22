/***
 * Name: test_codegen_shlex_lowering
 * Purpose: Verify lowering of shlex.split/join.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="shx.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenShlex, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = shlex.split("a 'b c'")
  b = shlex.join(["a", "b c"]) 
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_shlex_split(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_shlex_join(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_shlex_split(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_shlex_join(ptr"), std::string::npos);
}

