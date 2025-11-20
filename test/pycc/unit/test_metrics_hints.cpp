/***
 * Name: test_metrics_hints
 * Purpose: Validate Metrics emits hints based on counters/gauges and optimizer stats.
 */
#include <gtest/gtest.h>
#include "observability/Metrics.h"

using namespace pycc::obs;

TEST(MetricsHints, HintsArrayPopulated) {
  Metrics m;
  m.start("X"); m.stop("X");
  m.setCounter("sema.diagnostics", 2);
  m.setOptimizerStat("folds", 0);
  m.setGauge("codegen.ir_bytes", 60001);
  auto js = m.summaryJson();
  ASSERT_NE(js.find("\"hints\""), std::string::npos);
  ASSERT_NE(js.find("sema_diagnostics_present"), std::string::npos);
  ASSERT_NE(js.find("optimizer_no_effect"), std::string::npos);
  ASSERT_NE(js.find("large_ir"), std::string::npos);
}

