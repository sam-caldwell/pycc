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
#include <ios>
#include <ostream>

namespace pycc::metrics {

static const char* PhaseName(Metrics::Phase phase) {
  switch (phase) {
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

auto Metrics::PrintMetrics(const Registry& reg, std::ostream& out) -> void {
  if (!reg.enabled) {
    return;
  }
  out << "== Metrics ==\n";
  for (const auto& entry : reg.durations_ns) {
    const auto phase = entry.first;
    const auto nanoseconds = entry.second;
    const double milliseconds = static_cast<double>(nanoseconds) / 1'000'000.0;
    out << "  " << PhaseName(phase) << ": " << std::fixed << std::setprecision(3) << milliseconds << " ms\n";
  }
  out << "  AST: nodes=" << reg.ast_geom.node_count << ", max_depth=" << reg.ast_geom.max_depth << "\n";
  out << "  Optimizations (" << reg.optimizations.size() << "):\n";
  for (const auto& note : reg.optimizations) {
    out << "    - " << note << "\n";
  }
}

}  // namespace pycc::metrics
