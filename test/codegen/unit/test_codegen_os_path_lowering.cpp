/***
 * Name: test_codegen_os_path_lowering
 * Purpose: Verify lowering of os.path subset functions.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR_osp(const char* src, const char* file="osp.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenOsPath, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  j = os.path.join('a', 'b')
  d = os.path.dirname('/tmp/x')
  b = os.path.basename('/tmp/x')
  s = os.path.splitext('/tmp/x.txt')
  a = os.path.abspath('x')
  e = os.path.exists('/')
  f = os.path.isfile('/')
  g = os.path.isdir('/')
  return 0
)PY";
  auto ir = genIR_osp(src);
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

