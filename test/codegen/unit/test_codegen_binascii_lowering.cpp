/***
 * Name: test_codegen_binascii_lowering
 * Purpose: Verify lowering of binascii.hexlify/unhexlify.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="ba.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenBinascii, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = binascii.hexlify("hi")
  b = binascii.unhexlify("6869")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_binascii_hexlify(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_binascii_unhexlify(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_binascii_hexlify(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_binascii_unhexlify(ptr"), std::string::npos);
}

