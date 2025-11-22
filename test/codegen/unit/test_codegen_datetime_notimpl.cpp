/***
 * Name: test_codegen_datetime_notimpl
 * Purpose: Verify unknown datetime attribute lowers to a runtime raise.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="dt_notimpl.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenDatetime, UnknownAttrRaises) {
  const char* src = R"PY(
def main() -> int:
  a = datetime.not_a_func()
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("pycc_rt_raise"), std::string::npos);
}

