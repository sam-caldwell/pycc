/***
 * Name: test_list_append_call
 * Purpose: Verify lowering of xs.append(v) to list_push.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "append.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, ListAppendLowering) {
  const char* src =
      "def main() -> int:\n"
      "  xs = [1]\n"
      "  xs.append(2)\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("call void @pycc_list_push(ptr"), std::string::npos);
}

