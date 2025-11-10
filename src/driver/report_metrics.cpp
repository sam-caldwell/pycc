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

namespace pycc::driver {

auto ReportMetricsIfRequested(const driver::CliOptions& opts) -> void {
  if (!opts.metrics) {
    return;
  }
  const auto& reg = metrics::Metrics::GetRegistry();
  if (opts.metrics_format == driver::CliOptions::MetricsFormat::Json) {
    metrics::Metrics::PrintMetricsJson(reg, std::cout);
  } else {
    metrics::Metrics::PrintMetrics(reg, std::cout);
  }
}

}  // namespace pycc::driver
