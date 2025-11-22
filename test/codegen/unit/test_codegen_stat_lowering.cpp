/***
 * Name: test_codegen_stat_lowering
 * Purpose: Verify lowering of stat.S_IFMT/S_ISDIR/S_ISREG.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="statm.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenStat, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = stat.S_IFMT(0)
  b = stat.S_ISDIR(0)
  c = stat.S_ISREG(0)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare i32 @pycc_stat_ifmt(i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_stat_isdir(i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_stat_isreg(i32)"), std::string::npos);
  ASSERT_NE(ir.find("call i32 @pycc_stat_ifmt(i32"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_stat_isdir(i32"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_stat_isreg(i32"), std::string::npos);
}

