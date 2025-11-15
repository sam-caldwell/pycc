/***
 * Name: pycc::obs::Metrics (impl)
 * Purpose: Implement simple timing and formatting.
 */
#include "observability/Metrics.h"
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

namespace pycc::obs {

namespace {
constexpr double kUsPerMs = 1000.0;
constexpr int kIndent4 = 4;
constexpr int kIndent6 = 6;
} // namespace

static void appendDurations(std::ostringstream& oss,
                            const std::map<std::string, uint64_t>& durations) {
  oss << "  \"durations_ms\": {";
  bool first = true;
  for (const auto& [key, val] : durations) {
    if (!first) { oss << ","; }
    first = false;
    const double millis = static_cast<double>(val) / kUsPerMs;
    oss << "\n    \"" << key << "\": " << std::fixed << std::setprecision(3) << millis;
  }
  oss << "\n  }";
}

static void appendAst(std::ostringstream& oss, const std::optional<AstGeometry>& geom) {
  if (!geom) { return; }
  oss << ",\n  \"ast\": { \"nodes\": " << geom->nodes
      << ", \"max_depth\": " << geom->maxDepth << " }";
}

static void appendKeyValueObject(std::ostringstream& oss,
                                 const std::unordered_map<std::string, uint64_t>& values,
                                 int indent) {
  const std::string pad(indent, ' ');
  bool first = true;
  for (const auto& [key, val] : values) {
    if (!first) { oss << ","; }
    first = false;
    oss << "\n" << pad << "\"" << key << "\": " << val;
  }
}

static void appendOptimizer(std::ostringstream& oss,
                            const std::unordered_map<std::string, uint64_t>& stats) {
  if (stats.empty()) { return; }
  oss << ",\n  \"optimizer\": {";
  appendKeyValueObject(oss, stats, kIndent4);
  oss << "\n  }";
}

static void appendOptimizerBreakdown(
    std::ostringstream& oss,
    const std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>>& breakdown) {
  if (breakdown.empty()) { return; }
  oss << ",\n  \"optimizer_breakdown\": {";
  bool firstPass = true;
  for (const auto& [pass, passMap] : breakdown) {
    if (!firstPass) { oss << ","; }
    firstPass = false;
    oss << "\n    \"" << pass << "\": {";
    appendKeyValueObject(oss, passMap, kIndent6);
    oss << "\n    }";
  }
  oss << "\n  }";
}

void Metrics::start(const std::string& name) {
  active_[name] = Clock::now();
}


void Metrics::stop(const std::string& name) {
  auto iter = active_.find(name);
  if (iter == active_.end()) { return; }
  auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - iter->second).count();
  durations_us_[name] += static_cast<uint64_t>(microseconds);
  active_.erase(iter);
}

std::string Metrics::summaryText() const {
  std::ostringstream oss;
  oss << "== Metrics ==\n";
  for (const auto& [key, val] : durations_us_) {
    const double millis = static_cast<double>(val) / kUsPerMs;
    oss << "  " << key << ": " << std::fixed << std::setprecision(3) << millis << " ms\n";
  }
  if (geom_) {
    oss << "  AST: nodes=" << geom_->nodes << ", max_depth=" << geom_->maxDepth << "\n";
  }
  return oss.str();
}

std::string Metrics::summaryJson() const {
  std::ostringstream oss;
  oss << "{\n";
  appendDurations(oss, durations_us_);
  appendAst(oss, geom_);
  appendOptimizer(oss, optimizerStats_);
  appendOptimizerBreakdown(oss, optimizerBreakdown_);
  oss << "\n}\n";
  return oss.str();
}

} // namespace pycc::obs
