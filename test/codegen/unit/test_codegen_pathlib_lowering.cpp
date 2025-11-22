/***
 * Name: test_codegen_pathlib_lowering
 * Purpose: Verify pathlib lowering and declarations are present.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="pathlib_codegen.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenPathlib, DeclaresAndCalls) {
  const char* src = R"PY(
import pathlib
def main() -> int:
  a = pathlib.cwd()
  b = pathlib.join("a","b")
  return 0
)PY";
  auto ir = genIR(src);
  // Declarations
  ASSERT_NE(ir.find("declare ptr @pycc_pathlib_cwd()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_pathlib_join2(ptr, ptr)"), std::string::npos);
  // Calls present for cwd and join
  ASSERT_NE(ir.find("call ptr @pycc_pathlib_cwd()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_pathlib_join2(ptr"), std::string::npos);
  // Accept that some calls may not appear in this minimal snippet
}

TEST(CodegenPathlib, UnknownAttrRaises) {
  const char* src = R"PY(
import pathlib
def main() -> int:
  x = pathlib.not_a_func("x")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare void @pycc_rt_raise(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_rt_raise(ptr"), std::string::npos);
}
