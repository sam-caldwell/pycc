/***
 * Name: test_codegen_string_lowering
 * Purpose: Verify lowering of string.capwords.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="strmod.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenString, DeclaresAndCallsCapwords) {
  const char* src = R"PY(
def main() -> int:
  a = string.capwords("hello world")
  b = string.capwords("h-e-l-l-o", "-")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_string_capwords(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_string_capwords(ptr"), std::string::npos);
}

