/***
 * Name: test_codegen_posixpath_lowering
 * Purpose: Verify lowering of posixpath subset functions.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR_pp(const char* src, const char* file="pp.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenPosixpath, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  j = posixpath.join('a', 'b')
  d = posixpath.dirname('/tmp/x')
  b = posixpath.basename('/tmp/x')
  s = posixpath.splitext('/tmp/x.txt')
  a = posixpath.abspath('x')
  e = posixpath.exists('/')
  f = posixpath.isfile('/')
  g = posixpath.isdir('/')
  return 0
)PY";
  auto ir = genIR_pp(src);
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

