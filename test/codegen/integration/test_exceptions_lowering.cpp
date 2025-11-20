/***
 * Name: test_exceptions_lowering
 * Purpose: Verify codegen lowers raise and try/except using runtime exception APIs.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "exc.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, RaiseExceptionHasRuntimeCall) {
  const char* src =
      "def main() -> int:\n"
      "  raise Exception(\"boom\")\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("declare void @pycc_rt_raise(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_rt_raise(ptr"), std::string::npos);
}

TEST(CodegenIR, TryExceptUsesExceptionAPI) {
  const char* src =
      "def main() -> int:\n"
      "  x = 0\n"
      "  try:\n"
      "    x = 1\n"
      "    raise Exception(\"e\")\n"
      "  except Exception as e:\n"
      "    x = 2\n"
      "  else:\n"
      "    x = 3\n"
      "  finally:\n"
      "    x = x\n"
      "  return x\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // Presence of exception API calls
  ASSERT_NE(ir.find("declare i1 @pycc_rt_has_exception()"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_rt_has_exception()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_rt_current_exception()"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_rt_clear_exception()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_rt_exception_type(ptr"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_string_eq(ptr, ptr)"), std::string::npos);
}

