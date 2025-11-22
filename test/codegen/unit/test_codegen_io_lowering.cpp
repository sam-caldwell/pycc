/***
 * Name: test_codegen_io_lowering
 * Purpose: Verify io.* lowering to runtime shims and declarations.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="io_full.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenIO, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  io.write_stdout("hello")
  io.write_stderr("oops")
  c = io.read_file("/dev/null")
  ok = io.write_file("/tmp/pycc-io-test", "data")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare void @pycc_io_write_stdout(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_io_write_stderr(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_io_read_file(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_io_write_file(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_io_write_stdout(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_io_write_stderr(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_io_read_file(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_io_write_file(ptr"), std::string::npos);
}

