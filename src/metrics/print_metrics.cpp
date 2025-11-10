/***
 * Name: pycc::metrics::PrintMetrics
 * Purpose: Pretty-print collected metrics (durations, AST geometry, optimizations).
 * Inputs:
 *   - reg: metrics registry
 *   - out: destination stream
 * Outputs: None
 * Theory of Operation: Formats timings in milliseconds and lists counters.
 */
#include "pycc/metrics/metrics.h"

#include <iomanip>
#include <ostream>

namespace pycc {
namespace metrics {

static const char* PhaseName(Metrics::Phase p) {
  switch (p) {
    case Metrics::Phase::ReadFile: return "ReadFile";
    case Metrics::Phase::Parse: return "Parse";
    case Metrics::Phase::Sema: return "Sema";
    case Metrics::Phase::EmitIR: return "EmitIR";
    case Metrics::Phase::EmitASM: return "EmitASM";
    case Metrics::Phase::Compile: return "Compile";
    case Metrics::Phase::Link: return "Link";
  }
  return "Unknown";
}

void Metrics::PrintMetrics(const Registry& reg, std::ostream& out) {
  if (!reg.enabled) return;
  out << "== Metrics ==\n";
  for (const auto& [phase, ns] : reg.durations_ns) {
    double ms = static_cast<double>(ns) / 1'000'000.0;
    out << "  " << PhaseName(phase) << ": " << std::fixed << std::setprecision(3) << ms << " ms\n";
  }
  out << "  AST: nodes=" << reg.ast_geom.node_count << ", max_depth=" << reg.ast_geom.max_depth << "\n";
  out << "  Optimizations (" << reg.optimizations.size() << "):\n";
  for (const auto& opt : reg.optimizations) {
    out << "    - " << opt << "\n";
  }
}

}  // namespace metrics
}  // namespace pycc
