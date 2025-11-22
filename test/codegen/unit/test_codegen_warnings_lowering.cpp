/***
 * Name: test_codegen_warnings_lowering
 * Purpose: Verify lowering of warnings.warn/simplefilter.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="warn.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenWarnings, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  warnings.warn("oops")
  warnings.simplefilter("ignore")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare void @pycc_warnings_warn(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_warnings_simplefilter(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_warnings_warn(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_warnings_simplefilter(ptr, ptr)"), std::string::npos);
}

