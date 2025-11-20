/***
 * Name: test_codegen_list_subscript
 * Purpose: Verify codegen emits list get/set calls for subscripts.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "list_sub.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, ListSubscriptLoadAndStore) {
  const char* src =
      "def main() -> int:\n"
      "  xs = [1, 2, 3]\n"
      "  y = xs[0]\n"
      "  xs[1] = 42\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("declare ptr @pycc_list_get(ptr, i64)"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_list_set(ptr, i64, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_list_get(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_list_set(ptr"), std::string::npos);
}

