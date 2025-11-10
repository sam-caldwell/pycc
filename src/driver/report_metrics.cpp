/***
 * Name: pycc::driver::ReportMetricsIfRequested
 * Purpose: Print metrics to stdout if enabled by CLI.
 * Inputs:
 *   - opts: CLI options containing metrics flags
 * Outputs: None
 * Theory of Operation: Reads Metrics registry and prints text or JSON.
 */
#include "pycc/driver/app.h"
#include "pycc/metrics/metrics.h"

#include <iostream>

namespace pycc {
namespace driver {

void ReportMetricsIfRequested(const driver::CliOptions& opts) {
  if (!opts.metrics) return;
  const auto& reg = metrics::Metrics::GetRegistry();
  if (opts.metrics_format == driver::CliOptions::MetricsFormat::Json) {
    metrics::Metrics::PrintMetricsJson(reg, std::cout);
  } else {
    metrics::Metrics::PrintMetrics(reg, std::cout);
  }
}

}  // namespace driver
}  // namespace pycc

