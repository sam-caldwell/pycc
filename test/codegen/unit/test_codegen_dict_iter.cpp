/***
 * Name: test_codegen_dict_iter
 * Purpose: Verify dict iteration lowers to runtime iterator helpers.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src) {
  lex::Lexer L; L.pushString(src, "dictiter.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenDict, IterNewAndNextCalls) {
  const char* src = R"PY(
def main() -> int:
  d = {"a": 1}
  for k in d:
    pass
  return 0
)PY";
  const auto ir = genIR(src);
  ASSERT_NE(ir.find("@pycc_dict_iter_new"), std::string::npos);
  ASSERT_NE(ir.find("@pycc_dict_iter_next"), std::string::npos);
}

