/***
 * Name: test_codegen_hmac_lowering
 * Purpose: Verify lowering of hmac.digest.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="hmacm.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenHmac, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  d = hmac.digest("key", "msg", "sha256")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_hmac_digest(ptr, ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_hmac_digest(ptr"), std::string::npos);
}

