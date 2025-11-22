/***
 * Name: test_codegen_secrets_lowering
 * Purpose: Verify lowering of secrets.token_*.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="secm.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenSecrets, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = secrets.token_bytes(8)
  b = secrets.token_hex(8)
  c = secrets.token_urlsafe(8)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_secrets_token_bytes(i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_secrets_token_hex(i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_secrets_token_urlsafe(i32)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_secrets_token_bytes(i32"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_secrets_token_hex(i32"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_secrets_token_urlsafe(i32"), std::string::npos);
}

