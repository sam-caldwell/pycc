/***
 * Name: test_codegen_fnmatch_lowering
 * Purpose: Verify lowering of fnmatch module API.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="fm.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenFnmatch, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = fnmatch.fnmatch("ab", "a?")
  b = fnmatch.fnmatchcase("ab", "a?")
  c = fnmatch.translate("a*")
  d = fnmatch.filter(["a", "ab"], "a*")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare i1 @pycc_fnmatch_fnmatch(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_fnmatch_fnmatchcase(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_fnmatch_translate(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_fnmatch_filter(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_fnmatch_fnmatch(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_fnmatch_fnmatchcase(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_fnmatch_translate(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_fnmatch_filter(ptr"), std::string::npos);
}

