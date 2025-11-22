/***
 * Name: test_codegen_uuid_lowering
 * Purpose: Verify lowering of uuid.uuid4.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="uuidm.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenUUID, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  u = uuid.uuid4()
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_uuid_uuid4()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_uuid_uuid4()"), std::string::npos);
}

