/***
 * Name: pycc::obs::Metrics
 * Purpose: Collect simple per-stage timings and AST geometry for visibility.
 * Inputs:
 *   - Calls to Start/Stop timers for named stages.
 *   - AST summary values (nodes, depth) recorded by the compiler.
 * Outputs:
 *   - Human-readable text and JSON summaries.
 * Theory of Operation:
 *   Uses steady_clock timestamps to measure durations. Stores a map from
 *   stage names to microseconds. Geometry is a small struct. Formatting is
 *   performed on demand.
 */
#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>

namespace pycc::obs {

struct AstGeometry {
  uint64_t nodes{0};
  uint64_t maxDepth{0};
};

  class Metrics {
 public:
  using Clock = std::chrono::steady_clock;

  void start(const std::string& name);
  void stop(const std::string& name);

  void setAstGeometry(AstGeometry g) { geom_ = g; }
  const std::optional<AstGeometry>& astGeometry() const { return geom_; }

  std::string summaryText() const;
  std::string summaryJson() const;

 private:
  std::map<std::string, Clock::time_point> active_{};
  std::map<std::string, uint64_t> durations_us_{};
  std::optional<AstGeometry> geom_{};
  std::unordered_map<std::string, uint64_t> optimizerStats_{};
  std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> optimizerBreakdown_{};
  std::unordered_map<std::string, uint64_t> counters_{};
  std::unordered_map<std::string, uint64_t> gauges_{};

 public:
  void setOptimizerStat(const std::string& key, uint64_t value) { optimizerStats_[key] = value; }
  const std::unordered_map<std::string, uint64_t>& optimizerStats() const { return optimizerStats_; }
  void incOptimizerBreakdown(const std::string& pass, const std::string& key, uint64_t delta) { optimizerBreakdown_[pass][key] += delta; }
  const auto& optimizerBreakdown() const { return optimizerBreakdown_; }

  // Generic counters/gauges for observability
  void incCounter(const std::string& key, uint64_t delta = 1) { counters_[key] += delta; }
  void setCounter(const std::string& key, uint64_t value) { counters_[key] = value; }
  void setGauge(const std::string& key, uint64_t value) { gauges_[key] = value; }
  const auto& counters() const { return counters_; }
  const auto& gauges() const { return gauges_; }
};

} // namespace pycc::obs
