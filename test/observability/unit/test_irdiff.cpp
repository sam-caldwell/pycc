/***
 * Name: test_irdiff
 * Purpose: Validate IR diff ignores debug and comments and reports instruction differences.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"
#include "observability/IRDiff.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseText(const char* src) {
  lex::Lexer L; L.pushString(src, "diff_test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ObservabilityIRDiff, ReportsReturnDifferencesIgnoringDebug) {
  const char* srcA = "def main() -> int:\n  return 5\n";
  const char* srcB = "def main() -> int:\n  return 6\n";
  auto modA = parseText(srcA);
  auto modB = parseText(srcB);
  auto irA = codegen::Codegen::generateIR(*modA);
  auto irB = codegen::Codegen::generateIR(*modB);
  auto d = obs::diffIR(irA, irB, /*ignoreComments=*/true, /*ignoreDebug=*/true);
  ASSERT_NE(d.find("- ret i32 5"), std::string::npos);
  ASSERT_NE(d.find("+ ret i32 6"), std::string::npos);
}

