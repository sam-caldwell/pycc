/***
 * Name: test_codegen_dict_and_attr
 * Purpose: Verify codegen emits dict literal and attribute access IR calls.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "dict_attr.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, DictLiteralAndAttrAccess) {
  const char* src =
      "def main() -> int:\n"
      "  d = {1: 2, 3: 4}\n"
      "  o = object(2)\n"
      "  x = o.foo\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // Dict support declarations and calls
  ASSERT_NE(ir.find("declare ptr @pycc_dict_new(i64)"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_dict_set(ptr"), std::string::npos);
  // Attribute access should allocate a String and call object_get_attr
  ASSERT_NE(ir.find("declare ptr @pycc_string_new(ptr, i64)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_object_get_attr(ptr"), std::string::npos);
}

