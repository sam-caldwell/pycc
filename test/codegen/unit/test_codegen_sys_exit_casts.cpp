/***
 * Name: test_codegen_sys_exit_casts
 * Purpose: Ensure sys.exit casts bool/float to i32.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="sys_exit_casts.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenSys, ExitCastsBoolAndFloatToI32) {
  const char* src = R"PY(
def main() -> int:
  sys.exit(True)
  sys.exit(1.25)
  return 0
)PY";
  auto ir = genIR(src);
  // zext i1 -> i32
  ASSERT_NE(ir.find("zext i1"), std::string::npos);
  // fptosi double -> i32
  ASSERT_NE(ir.find("fptosi double"), std::string::npos);
}

