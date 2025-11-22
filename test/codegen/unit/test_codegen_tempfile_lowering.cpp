/***
 * Name: test_codegen_tempfile_lowering
 * Purpose: Verify lowering of tempfile.gettempdir/mkdtemp/mkstemp.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="tmpf.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenTempfile, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = tempfile.gettempdir()
  b = tempfile.mkdtemp()
  c = tempfile.mkstemp()
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_tempfile_gettempdir()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_tempfile_mkdtemp()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_tempfile_mkstemp()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_tempfile_gettempdir()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_tempfile_mkdtemp()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_tempfile_mkstemp()"), std::string::npos);
}

