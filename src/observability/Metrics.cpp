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

static std::string to_lower_copy(std::string s) {
  for (auto& c : s) { if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a'); }
  return s;
}

static void appendDurations(std::ostringstream& oss,
                            const std::map<std::string, uint64_t>& durations) {
  oss << "  \"durations_ms\": {";
  bool first = true;
  for (const auto& [key, val] : durations) {
    if (!first) { oss << ","; }
    first = false;
    const double millis = static_cast<double>(val) / kUsPerMs;
    // JSON uses lowercase stage keys for stability (e2e contracts)
    oss << "\n    \"" << to_lower_copy(key) << "\": " << std::fixed << std::setprecision(3) << millis;
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
  if (!counters_.empty()) {
    oss << ",\n  \"counters\": {";
    appendKeyValueObject(oss, counters_, kIndent4);
    oss << "\n  }";
  }
  if (!gauges_.empty()) {
    oss << ",\n  \"gauges\": {";
    appendKeyValueObject(oss, gauges_, kIndent4);
    oss << "\n  }";
  }
  auto hs = hints();
  if (!hs.empty()) {
    oss << ",\n  \"hints\": [";
    for (size_t i = 0; i < hs.size(); ++i) {
      if (i != 0) oss << ", ";
      oss << "\"" << hs[i] << "\"";
    }
    oss << "]";
  }
  oss << "\n}\n";
  return oss.str();
}

std::vector<std::string> Metrics::hints() const {
  std::vector<std::string> out;
  auto itDiag = counters_.find("sema.diagnostics");
  if (itDiag != counters_.end() && itDiag->second > 0) { out.emplace_back("sema_diagnostics_present"); }
  auto itFolds = optimizerStats_.find("folds");
  if (itFolds != optimizerStats_.end()) { out.emplace_back(itFolds->second > 0 ? "optimizer_effective" : "optimizer_no_effect"); }
  auto itIR = gauges_.find("codegen.ir_bytes");
  if (itIR != gauges_.end() && itIR->second > 50000U) { out.emplace_back("large_ir"); }
  return out;
}

} // namespace pycc::obs
