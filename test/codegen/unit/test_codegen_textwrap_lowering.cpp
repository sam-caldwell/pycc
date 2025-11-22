/***
 * Name: test_codegen_textwrap_lowering
 * Purpose: Verify lowering of textwrap.fill/shorten.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="tw.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenTextwrap, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = textwrap.fill("This is a test", 6)
  b = textwrap.shorten("This is a test", 8)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_textwrap_fill(ptr, i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_textwrap_shorten(ptr, i32)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_textwrap_fill(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_textwrap_shorten(ptr"), std::string::npos);
}

