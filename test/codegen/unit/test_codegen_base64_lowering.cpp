/***
 * Name: test_codegen_base64_lowering
 * Purpose: Verify lowering of base64.b64encode/b64decode.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="b64.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenBase64, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = base64.b64encode("hi")
  b = base64.b64decode(a)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_base64_b64encode(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_base64_b64decode(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_base64_b64encode(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_base64_b64decode(ptr"), std::string::npos);
}

