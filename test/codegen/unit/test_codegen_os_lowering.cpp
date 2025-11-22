/***
 * Name: test_codegen_os_lowering
 * Purpose: Verify lowering of os.* helpers.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="osmod.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenOS, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = os.getcwd()
  b = os.mkdir("_tmp_dir")
  c = os.remove("_tmp_dir/nonexistent")
  d = os.rename("a", "b")
  e = os.getenv("PATH")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_os_getcwd()"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_os_mkdir(ptr, i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_os_remove(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_os_rename(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_os_getenv(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_os_getcwd()"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_os_mkdir(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_os_remove(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_os_rename(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_os_getenv(ptr"), std::string::npos);
}

