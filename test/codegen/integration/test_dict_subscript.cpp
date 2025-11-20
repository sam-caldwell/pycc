/***
 * Name: test_dict_subscript
 * Purpose: Verify dict subscript get/set lowering.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "dict_sub.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, DictGetSet) {
  const char* src =
      "def main() -> int:\n"
      "  d = {\"k\": \"v\"}\n"
      "  v = d[\"k\"]\n"
      "  d[\"x\"] = \"y\"\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("declare ptr @pycc_dict_get(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_dict_get(ptr"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_dict_set(ptr, ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_dict_set(ptr"), std::string::npos);
}

