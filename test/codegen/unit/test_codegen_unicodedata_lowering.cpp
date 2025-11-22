/***
 * Name: test_codegen_unicodedata_lowering
 * Purpose: Verify lowering of unicodedata.normalize.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR_ud(const char* src, const char* file="ud.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenUnicodedata, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = unicodedata.normalize('NFC', 'cafe')
  return 0
)PY";
  auto ir = genIR_ud(src);
  ASSERT_NE(ir.find("declare ptr @pycc_unicodedata_normalize(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_unicodedata_normalize(ptr"), std::string::npos);
}

