/***
 * Name: test_codegen_ntpath_lowering
 * Purpose: Verify lowering of ntpath subset functions.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR_ntp(const char* src, const char* file="ntp.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenNtpath, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  j = ntpath.join('a', 'b')
  d = ntpath.dirname('C:/tmp/x')
  b = ntpath.basename('C:/tmp/x')
  s = ntpath.splitext('C:/tmp/x.txt')
  a = ntpath.abspath('x')
  e = ntpath.exists('/')
  f = ntpath.isfile('/')
  g = ntpath.isdir('/')
  return 0
)PY";
  auto ir = genIR_ntp(src);
  ASSERT_NE(ir.find("declare ptr @pycc_os_path_join2(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_os_path_dirname(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_os_path_basename(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_os_path_splitext(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_os_path_abspath(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_os_path_exists(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_os_path_isfile(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_os_path_isdir(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_os_path_join2(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_os_path_dirname(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_os_path_basename(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_os_path_splitext(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_os_path_abspath(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_os_path_exists(ptr"), std::string::npos);
}

