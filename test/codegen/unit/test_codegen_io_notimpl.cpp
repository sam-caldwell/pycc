/***
 * Name: test_codegen_io_notimpl
 * Purpose: Verify unknown io attribute lowers to a runtime raise (NotImplementedError).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="io_notimpl.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenIO, UnknownAttrRaises) {
  const char* src = R"PY(
def main() -> int:
  x = io.not_a_func("x")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("pycc_rt_raise"), std::string::npos);
}

