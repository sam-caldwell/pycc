/***
 * Name: test_codegen_pprint_lowering
 * Purpose: Verify lowering of pprint.pformat.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="pp.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenPprint, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = pprint.pformat([1,2,3])
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_pprint_pformat(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_pprint_pformat(ptr"), std::string::npos);
}

