/***
 * Name: test_codegen_subprocess_notimpl
 * Purpose: Verify unknown subprocess attribute lowers to a runtime raise (NotImplementedError).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="sp_notimpl.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenSubprocess, UnknownAttrRaisesNotImplemented) {
  const char* src = R"PY(
import subprocess
def main() -> int:
  a = subprocess.not_a_func("true")
  return 0
)PY";
  auto ir = genIR(src);
  // IR should contain a raise with this message embedded as a global string
  ASSERT_NE(ir.find("stdlib subprocess.not_a_func not implemented"), std::string::npos);
  ASSERT_NE(ir.find("pycc_rt_raise"), std::string::npos);
}

