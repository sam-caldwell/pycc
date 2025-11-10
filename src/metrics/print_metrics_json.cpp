/***
 * Name: pycc::metrics::PrintMetricsJson
 * Purpose: Print metrics in JSON for consumption by tools.
 * Inputs:
 *   - reg: metrics registry
 *   - out: destination stream
 * Outputs: None
 * Theory of Operation: Simple JSON writer; escapes strings and prints arrays of objects.
 */
#include "pycc/metrics/metrics.h"

#include <ostream>
#include <string>

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

static std::string JsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[7];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

void Metrics::PrintMetricsJson(const Registry& reg, std::ostream& out) {
  if (!reg.enabled) return;
  out << "{";
  // durations
  out << "\n  \"durations_ns\": [";
  for (size_t i = 0; i < reg.durations_ns.size(); ++i) {
    const auto& item = reg.durations_ns[i];
    out << (i ? ",\n    {" : "\n    {")
        << "\"phase\": \"" << PhaseName(item.first) << "\", \"ns\": " << item.second << "}";
  }
  out << "\n  ],";
  // AST
  out << "\n  \"ast\": { \"nodes\": " << reg.ast_geom.node_count
      << ", \"max_depth\": " << reg.ast_geom.max_depth << " },";
  // optimizations
  out << "\n  \"optimizations\": [";
  for (size_t i = 0; i < reg.optimizations.size(); ++i) {
    out << (i ? ", " : " ") << "\"" << JsonEscape(reg.optimizations[i]) << "\"";
  }
  out << " ]\n}";
}

}  // namespace metrics
}  // namespace pycc
