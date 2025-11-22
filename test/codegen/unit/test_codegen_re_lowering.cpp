/***
 * Name: test_codegen_re_lowering
 * Purpose: Verify lowering of re module API into runtime shims and declarations.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="re_lowering.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenRe, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = re.compile("a+")
  m = re.search("a+", "baaa")
  f = re.findall("a+", "baaa")
  it = re.finditer("a+", "baaa")
  s = re.split(",", "a,b,c", 1)
  r = re.sub("a+", "x", "baaa", 1)
  e = re.escape("a+b")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_re_compile(ptr, i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_re_search(ptr, ptr, i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_re_findall(ptr, ptr, i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_re_split(ptr, ptr, i32, i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_re_sub(ptr, ptr, ptr, i32, i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_re_escape(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_re_finditer(ptr, ptr, i32)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_re_compile"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_re_search"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_re_findall"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_re_split"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_re_sub"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_re_escape"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_re_finditer"), std::string::npos);
}
