/***
 * Name: test_codegen_json_lowering
 * Purpose: Verify json.dumps/loads lowering and declarations.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="json_full.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenJSON, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  s = json.dumps([1,2])
  v = json.loads("{\"a\":1}")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_json_dumps(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_json_loads(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_json_dumps(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_json_loads(ptr"), std::string::npos);
}

TEST(CodegenJSON, DumpsWithIndentCallsEx) {
  const char* src = R"PY(
def main() -> int:
  s = json.dumps([1,2], 2)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_json_dumps_ex(ptr, i32)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_json_dumps_ex(ptr"), std::string::npos);
}
