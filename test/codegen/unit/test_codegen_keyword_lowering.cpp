/***
 * Name: test_codegen_keyword_lowering
 * Purpose: Verify lowering of keyword module API.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="kw.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenKeyword, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = keyword.iskeyword("for")
  b = keyword.kwlist()
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare i1 @pycc_keyword_iskeyword(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_keyword_kwlist()"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_keyword_iskeyword(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_keyword_kwlist()"), std::string::npos);
}

