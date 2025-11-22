/***
 * Name: test_codegen_statistics_extras_lowering
 * Purpose: Verify lowering of statistics.stdev/pvariance.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR_statx(const char* src, const char* file="statx.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenStatisticsExtras, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  s = statistics.stdev([1,2,3])
  v = statistics.pvariance([1,2,3])
  return 0
)PY";
  auto ir = genIR_statx(src);
  ASSERT_NE(ir.find("declare double @pycc_statistics_stdev(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @pycc_statistics_pvariance(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call double @pycc_statistics_stdev(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call double @pycc_statistics_pvariance(ptr"), std::string::npos);
}

