/**
 * Name: test_list_len_name
 * Purpose: Verify lowering of list literal to name then len(name) IR.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, ListLenName) {
  const char* src =
      "def main() -> int:\n"
      "  a = [1,2,3]\n"
      "  return len(a)\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // Expect calls to list_new and list_push and list_len
  ASSERT_NE(ir.find("declare ptr @pycc_list_new(i64)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_list_new(i64 3)"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_list_push(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i64 @pycc_list_len(ptr"), std::string::npos);
  ASSERT_NE(ir.find("ret i32"), std::string::npos);
}

