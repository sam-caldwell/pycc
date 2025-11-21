/***
 * Name: test_range_analysis_basic
 * Purpose: Validate RangeAnalysis collects min/max for integer assignments.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/RangeAnalysis.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="ra.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(RangeAnalysis, CollectsMinMax) {
  const char* src = R"PY(
def main() -> int:
  x = 5
  x = 3
  y = 10
  return 0
)PY";
  auto mod = parseSrc(src);
  opt::RangeAnalysis ra;
  auto ranges = ra.analyze(*mod);
  ASSERT_TRUE(ranges.find("x") != ranges.end());
  EXPECT_EQ(ranges["x"].min, 3);
  EXPECT_EQ(ranges["x"].max, 5);
  ASSERT_TRUE(ranges.find("y") != ranges.end());
  EXPECT_EQ(ranges["y"].min, 10);
  EXPECT_EQ(ranges["y"].max, 10);
}

