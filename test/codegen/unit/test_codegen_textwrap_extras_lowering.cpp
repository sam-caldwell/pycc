/***
 * Name: test_codegen_textwrap_extras_lowering
 * Purpose: Verify lowering of textwrap.wrap/dedent.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR_EX(const char* src, const char* file="tw_extras.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenTextwrapExtras, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = textwrap.wrap("This is a test", 6)
  b = textwrap.dedent("  This\n    is")
  return 0
)PY";
  auto ir = genIR_EX(src);
  ASSERT_NE(ir.find("declare ptr @pycc_textwrap_wrap(ptr, i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_textwrap_dedent(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_textwrap_wrap(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_textwrap_dedent(ptr"), std::string::npos);
}

