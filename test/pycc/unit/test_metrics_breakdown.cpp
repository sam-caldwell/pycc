/***
 * Name: test_metrics_breakdown
 * Purpose: Verify Metrics summaryJson contains optimizer stats and breakdown.
 */
#include <gtest/gtest.h>
#include "observability/Metrics.h"

using namespace pycc::obs;

TEST(MetricsBreakdown, SummaryJsonShape) {
  Metrics m;
  m.start("StageA"); m.stop("StageA");
  m.setAstGeometry({10, 3});
  m.setOptimizerStat("folds", 3);
  m.incOptimizerBreakdown("constfold", "binary_int", 2);
  m.incOptimizerBreakdown("constfold", "unary", 1);

  auto js = m.summaryJson();
  EXPECT_NE(js.find("\"durations_ms\""), std::string::npos);
  EXPECT_NE(js.find("\"ast\""), std::string::npos);
  EXPECT_NE(js.find("\"optimizer\""), std::string::npos);
  EXPECT_NE(js.find("\"folds\": 3"), std::string::npos);
  EXPECT_NE(js.find("\"optimizer_breakdown\""), std::string::npos);
  EXPECT_NE(js.find("\"constfold\""), std::string::npos);
  EXPECT_NE(js.find("\"binary_int\": 2"), std::string::npos);
  EXPECT_NE(js.find("\"unary\": 1"), std::string::npos);
}

