/***
 * Name: test_codegen_getpass_lowering
 * Purpose: Verify lowering of getpass.getuser/getpass.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="gp.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenGetpass, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  u = getpass.getuser()
  p = getpass.getpass("pwd:")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_getpass_getuser()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_getpass_getpass(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_getpass_getuser()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_getpass_getpass(ptr"), std::string::npos);
}

