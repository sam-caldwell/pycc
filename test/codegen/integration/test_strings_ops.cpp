/***
 * Name: test_strings_ops
 * Purpose: Verify string concat and indexing lowering.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "str_ops.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, StringConcatAndIndex) {
  const char* src =
      "def main() -> str:\n"
      "  a = \"hi\"\n"
      "  b = \"!\"\n"
      "  c = a + b\n"
      "  d = a[0]\n"
      "  return c\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("declare ptr @pycc_string_concat(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_string_concat(ptr"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_string_slice(ptr, i64, i64)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_string_slice(ptr"), std::string::npos);
}

