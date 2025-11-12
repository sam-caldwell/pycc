/***
 * Name: pycc::obs::Metrics (impl)
 * Purpose: Implement simple timing and formatting.
 */
#include "observability/Metrics.h"
#include <iomanip>
#include <sstream>

namespace pycc::obs {

void Metrics::start(const std::string& name) {
  active_[name] = Clock::now();
}

void Metrics::stop(const std::string& name) {
  auto it = active_.find(name);
  if (it == active_.end()) return;
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - it->second).count();
  durations_us_[name] += static_cast<uint64_t>(us);
  active_.erase(it);
}

std::string Metrics::summaryText() const {
  std::ostringstream oss;
  oss << "== Metrics ==\n";
  for (const auto& [k, v] : durations_us_) {
    double ms = static_cast<double>(v) / 1000.0;
    oss << "  " << k << ": " << std::fixed << std::setprecision(3) << ms << " ms\n";
  }
  if (geom_) {
    oss << "  AST: nodes=" << geom_->nodes << ", max_depth=" << geom_->maxDepth << "\n";
  }
  return oss.str();
}

std::string Metrics::summaryJson() const {
  std::ostringstream oss;
  oss << "{\n  \"durations_ms\": {";
  bool first = true;
  for (const auto& [k, v] : durations_us_) {
    if (!first) oss << ",";
    first = false;
    double ms = static_cast<double>(v) / 1000.0;
    oss << "\n    \"" << k << "\": " << std::fixed << std::setprecision(3) << ms;
  }
  oss << "\n  }";
  if (geom_) {
    oss << ",\n  \"ast\": { \"nodes\": " << geom_->nodes << ", \"max_depth\": " << geom_->maxDepth << " }";
  }
  if (!optimizerStats_.empty()) {
    oss << ",\n  \"optimizer\": {";
    bool first2 = true;
    for (const auto& [k, v] : optimizerStats_) {
      if (!first2) oss << ",";
      first2 = false;
      oss << "\n    \"" << k << "\": " << v;
    }
    oss << "\n  }";
  }
  if (!optimizerBreakdown_.empty()) {
    oss << ",\n  \"optimizer_breakdown\": {";
    bool firstPass = true;
    for (const auto& [pass, mp] : optimizerBreakdown_) {
      if (!firstPass) oss << ",";
      firstPass = false;
      oss << "\n    \"" << pass << "\": {";
      bool firstK = true;
      for (const auto& [k, v] : mp) {
        if (!firstK) oss << ",";
        firstK = false;
        oss << "\n      \"" << k << "\": " << v;
      }
      oss << "\n    }";
    }
    oss << "\n  }";
  }
  oss << "\n}\n";
  return oss.str();
}

} // namespace pycc::obs
