/***
 * Name: test_codegen_argparse_lowering
 * Purpose: Verify lowering of argparse subset functions.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR_ap(const char* src, const char* file="ap.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenArgparse, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  p = argparse.ArgumentParser()
  argparse.add_argument(p, '--count', 'store_int')
  d = argparse.parse_args(p, ['--count', '3'])
  return 0
)PY";
  auto ir = genIR_ap(src);
  ASSERT_NE(ir.find("declare ptr @pycc_argparse_argument_parser()"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_argparse_add_argument(ptr, ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_argparse_parse_args(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_argparse_argument_parser()"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_argparse_add_argument(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_argparse_parse_args(ptr"), std::string::npos);
}

