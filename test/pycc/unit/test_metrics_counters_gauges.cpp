/***
 * Name: test_metrics_counters_gauges
 * Purpose: Validate counters and gauges are present in Metrics JSON.
 */
#include <gtest/gtest.h>
#include "observability/Metrics.h"

using namespace pycc::obs;

TEST(MetricsCountersGauges, IncludedInJson) {
  Metrics m;
  m.start("Stage"); m.stop("Stage");
  m.setCounter("parse.functions", 3);
  m.incCounter("parse.classes", 2);
  m.setGauge("sema.ok", 1);
  m.setGauge("codegen.ir_bytes", 1234);
  const auto js = m.summaryJson();
  ASSERT_NE(js.find("\"counters\""), std::string::npos);
  ASSERT_NE(js.find("\"parse.functions\": 3"), std::string::npos);
  ASSERT_NE(js.find("\"parse.classes\": 2"), std::string::npos);
  ASSERT_NE(js.find("\"gauges\""), std::string::npos);
  ASSERT_NE(js.find("\"sema.ok\": 1"), std::string::npos);
  ASSERT_NE(js.find("\"codegen.ir_bytes\": 1234"), std::string::npos);
}

