/***
 * Name: test_codegen_statistics_lowering
 * Purpose: Verify lowering of statistics.mean/median.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="stats.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenStatistics, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = statistics.mean([1,2,3])
  b = statistics.median([1,2,3,4])
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare double @pycc_statistics_mean(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare double @pycc_statistics_median(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call double @pycc_statistics_mean(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call double @pycc_statistics_median(ptr"), std::string::npos);
}

