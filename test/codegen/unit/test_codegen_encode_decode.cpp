/***
 * Name: test_codegen_encode_decode
 * Purpose: Verify str.encode(...) and bytes.decode(...) lower to runtime calls with defaults/literals.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="encdec.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenEncodeDecode, EmitsCalls) {
  const char* src = R"PY(
def main() -> int:
  s = "hi"
  b = s.encode("ascii", "strict")
  t = b.decode("utf-8", "replace")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_string_encode(ptr, ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_bytes_decode(ptr, ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_string_encode(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_bytes_decode(ptr"), std::string::npos);
}

